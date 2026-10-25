#include "minera/temporal_lazy_reference.hpp"

#include "python_str_classes.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace minera {
namespace {

struct Codepoint {
  char32_t value;
  std::string bytes;
};

std::vector<Codepoint> decode(const std::string& text) {
  std::vector<Codepoint> out;
  for (std::size_t at = 0; at < text.size();) {
    const auto lead = static_cast<unsigned char>(text[at]);
    const std::size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
    char32_t value = length == 1 ? lead : lead & (0x7f >> length);
    for (std::size_t k = 1; k < length && at + k < text.size(); ++k)
      value = (value << 6) | (static_cast<unsigned char>(text[at + k]) & 0x3f);
    out.push_back({value, text.substr(at, length)});
    at += length;
  }
  return out;
}

[[noreturn]] void raise(const std::string& type, const std::string& message) { throw TemporalLazyError(type, message); }

std::int64_t floor_div(std::int64_t value, std::int64_t divisor) {
  const auto quotient = value / divisor;
  return quotient - ((value % divisor != 0) && ((value < 0) != (divisor < 0)));
}

constexpr std::int64_t kDayMicros = 86'400'000'000;

// Civil date of a day count from 1970-01-01 and back.
void civil(std::int64_t days, std::int64_t& year, std::int64_t& month) {
  days += 719468;
  const auto era = floor_div(days, 146097);
  const auto doe = days - era * 146097;
  const auto yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const auto mp = (5 * doy + 2) / 153;
  month = mp < 10 ? mp + 3 : mp - 9;
  year = yoe + era * 400 + (month <= 2);
}

std::int64_t days_from_civil(std::int64_t year, std::int64_t month) {
  year -= month <= 2;
  const auto era = floor_div(year, 400);
  const auto yoe = year - era * 400;
  const auto doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5;
  const auto doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

std::int64_t truncate(std::int64_t micros, std::int64_t multiple, const std::string& unit) {
  if (multiple == 0) return micros;
  if (unit == "mo" || unit == "q" || unit == "y") {
    const auto width = (unit == "mo" ? 1 : unit == "q" ? 3 : 12) * multiple;
    std::int64_t year = 0, month = 0;
    civil(floor_div(micros, kDayMicros), year, month);
    const auto index = floor_div((year - 1970) * 12 + month - 1, width) * width;
    return days_from_civil(1970 + floor_div(index, 12), index - floor_div(index, 12) * 12 + 1) * kDayMicros;
  }
  if (unit == "w") {
    const auto offset = 4 * kDayMicros, width = 7 * kDayMicros * multiple;
    return floor_div(micros - offset, width) * width + offset;
  }
  const std::int64_t seconds = unit == "d" ? 86400 : unit == "h" ? 3600 : unit == "m" ? 60 : 1;
  const auto width = seconds * 1'000'000 * multiple;
  return floor_div(micros, width) * width;
}

bool case_less(const TemporalLazyCell& left, const TemporalLazyCell& right) {
  if (left.index() != right.index()) return left.index() < right.index();
  if (const auto* number = std::get_if<std::int64_t>(&left)) return *number < std::get<std::int64_t>(right);
  if (const auto* text = std::get_if<std::string>(&left)) return *text < std::get<std::string>(right);
  return false;
}

struct CaseLess {
  bool operator()(const TemporalLazyCell& left, const TemporalLazyCell& right) const { return case_less(left, right); }
};

}  // namespace

double temporal_lazy_kahan_sum(const std::vector<double>& values) {
  double sum = 0.0, compensation = 0.0;
  for (const double value : values) {
    const double corrected = value - compensation;
    const double next = sum + corrected;
    compensation = (next - sum) - corrected;
    sum = next;
  }
  return sum;
}

std::string reference_freq_to_polars_duration(const std::optional<std::string>& freq) {
  if (!freq || freq->empty()) raise("ValueError", "grouper frequency must be a non-empty string");
  auto points = decode(*freq);
  std::size_t first = 0, last = points.size();
  while (first < last && detail::python_isspace(points[first].value)) ++first;
  while (last > first && detail::python_isspace(points[last - 1].value)) --last;
  std::string stripped, digits, unit;
  bool foreign = false;
  bool in_base = true;
  for (std::size_t k = first; k < last; ++k) {
    const auto& point = points[k];
    stripped += point.bytes;
    if (point.value == U'-') in_base = false;
    if (!in_base) continue;
    if (detail::python_isdigit(point.value)) {
      digits += point.bytes;
    } else if (detail::python_isalpha(point.value)) {
      if (point.value < 0x80) unit += static_cast<char>(point.value >= 'a' && point.value <= 'z' ? point.value - 32 : point.value);
      else if (point.value == 0xdf) unit += "SS";
      else if (point.value == 0x131) unit += "I";
      else if (point.value == 0x17f) unit += "S";
      else { unit += '?'; foreign = true; }
    }
  }
  if (unit.empty()) raise("ValueError", "cannot infer unit from grouper frequency '" + stripped + "'");
  static const std::map<std::string, std::string> units = {
      {"S", "s"}, {"SEC", "s"}, {"SECOND", "s"}, {"T", "m"}, {"MIN", "m"}, {"M", "mo"}, {"D", "d"},
      {"H", "h"}, {"W", "w"}, {"Q", "q"}, {"Y", "y"}, {"A", "y"}};
  auto found = foreign ? units.end() : units.find(unit);
  for (const char suffix : {'S', 'E'}) {
    if (found != units.end() || foreign) break;
    while (!unit.empty() && unit.back() == suffix) unit.pop_back();
    found = units.find(unit);
  }
  if (found == units.end())
    raise("ValueError", "grouper frequency '" + stripped + "' is not supported when using the temporal_lazy variant.");
  return (digits.empty() ? "1" : digits) + found->second;
}

std::vector<TemporalLazyRow> reference_temporal_lazy(const TemporalLazyFrame& frame,
                                                     const TemporalLazyParameters& parameters) {

  const auto column = [&](const std::string& name) -> const TemporalLazyColumn& {
    for (const auto& candidate : frame.columns)
      if (candidate.name == name) return candidate;
    raise("ColumnNotFoundError", "unable to find column \"" + name + "\"");
  };
  // The case and time columns are looked up before the frequency is mapped,
  // the others before Polars parses the duration.
  const auto& cases_column = column(parameters.case_id_key);
  const auto& timestamps = column(parameters.timestamp_key);
  const auto& starts = column(parameters.start_timestamp_key.value_or(parameters.timestamp_key));
  const auto duration = reference_freq_to_polars_duration(parameters.grouper_freq);
  const auto& activities = column(parameters.activity_key);
  const auto& resources = column(parameters.resource_key);
  std::size_t split = 0;
  while (split < duration.size() && duration[split] >= '0' && duration[split] <= '9') ++split;
  const auto unit = duration.substr(split);
  if (split == 0 || (unit != "w" && unit != "d" && unit != "h" && unit != "m" && unit != "s" && unit != "mo" &&
                     unit != "q" && unit != "y"))
    raise("InvalidOperationError", "expected leading integer in the duration string");
  const auto multiple = std::stoll(duration.substr(0, split));
  const auto rows = cases_column.cells.size();
  const auto micros = [](const TemporalLazyColumn& times, std::size_t row) {
    const auto* value = std::get_if<std::int64_t>(&times.cells.at(row));
    if (!value) throw std::invalid_argument("temporal_lazy: null timestamps are outside the observed domain");
    return *value;
  };

  struct Case {
    std::int64_t start = 0, finish = 0;
    std::vector<double> services;
    double arrival = 0, finish_rate = 0, service = 0, sojourn = 0, waiting = 0;
  };
  std::map<TemporalLazyCell, Case, CaseLess> cases;
  for (std::size_t row = 0; row < rows; ++row) {
    const auto& identifier = cases_column.cells[row];
    if (std::holds_alternative<std::monostate>(identifier))
      throw std::invalid_argument("temporal_lazy: null case identifiers are outside the observed domain");
    const auto finish = micros(timestamps, row), start = micros(starts, row);
    const auto [entry, fresh] = cases.try_emplace(identifier);
    auto& metrics = entry->second;
    if (fresh) {
      metrics.start = start;
      metrics.finish = finish;
    }
    metrics.start = std::min(metrics.start, start);
    metrics.finish = std::max(metrics.finish, finish);
    const auto difference = static_cast<double>(finish - start);
    metrics.services.push_back(rows == 1 ? difference / 1e6 : difference * 1e-6);
  }
  for (const bool by_finish : {false, true}) {
    std::vector<std::pair<std::int64_t, const TemporalLazyCell*>> order;
    for (const auto& [identifier, metrics] : cases)
      order.emplace_back(by_finish ? metrics.finish : metrics.start, &identifier);
    std::stable_sort(order.begin(), order.end(),
                     [](const auto& left, const auto& right) { return left.first < right.first; });
    for (std::size_t k = 1; k < order.size(); ++k) {
      auto& metrics = cases.at(*order[k].second);
      (by_finish ? metrics.finish_rate : metrics.arrival) = static_cast<double>(order[k].first - order[k - 1].first) * 1e-6;
    }
  }
  for (auto& [identifier, metrics] : cases) {
    metrics.service = temporal_lazy_kahan_sum(metrics.services);
    const auto span = static_cast<double>(metrics.finish - metrics.start);
    metrics.sojourn = cases.size() == 1 ? span / 1e6 : span * 1e-6;
    metrics.waiting = metrics.sojourn - metrics.service;
  }

  std::map<std::int64_t, std::vector<std::size_t>> buckets;
  for (std::size_t row = 0; row < rows; ++row) buckets[truncate(micros(starts, row), multiple, unit)].push_back(row);
  std::vector<TemporalLazyRow> out;
  for (const auto& [label, members] : buckets) {
    TemporalLazyRow row;
    row.timestamp = label;
    std::vector<const TemporalLazyCell*> order;
    std::map<TemporalLazyCell, std::pair<std::set<TemporalLazyCell, CaseLess>, std::set<TemporalLazyCell, CaseLess>>,
             CaseLess> per_case;  // activities, resources
    std::set<TemporalLazyCell, CaseLess> all_activities, all_resources;
    std::map<TemporalLazyCell, std::set<TemporalLazyCell, CaseLess>, CaseLess> cases_by_resource;
    std::map<TemporalLazyCell, std::size_t, CaseLess> events_by_case;
    for (const auto member : members) {
      const auto& identifier = cases_column.cells[member];
      if (events_by_case[identifier]++ == 0) order.push_back(&identifier);
      auto& sets = per_case[identifier];
      if (const auto& activity = activities.cells[member]; !std::holds_alternative<std::monostate>(activity)) {
        all_activities.insert(activity);
        sets.first.insert(activity);
      }
      if (const auto& resource = resources.cells[member]; !std::holds_alternative<std::monostate>(resource)) {
        all_resources.insert(resource);
        sets.second.insert(resource);
        cases_by_resource[resource].insert(identifier);
      }
    }
    const auto count = static_cast<double>(order.size());
    for (const auto* identifier : order) {
      const auto& metrics = cases.at(*identifier);
      const double values[] = {metrics.arrival, metrics.finish_rate, metrics.waiting, metrics.sojourn, metrics.service};
      for (std::size_t k = 0; k < 5; ++k) row.case_values[k].push_back(values[k]);
      const auto& sets = per_case.at(*identifier);
      row.total_number_of_reworked_activities +=
          static_cast<std::uint32_t>(events_by_case.at(*identifier) - sets.first.size());
      row.avg_resources_per_case += static_cast<double>(sets.second.size());
    }
    row.unique_resources = static_cast<std::uint32_t>(all_resources.size());
    row.unique_activities = static_cast<std::uint32_t>(all_activities.size());
    row.num_events = static_cast<std::uint32_t>(members.size());
    row.average_arrival_rate = temporal_lazy_kahan_sum(row.case_values[0]) / count;
    row.average_finish_rate = temporal_lazy_kahan_sum(row.case_values[1]) / count;
    row.average_waiting_time = temporal_lazy_kahan_sum(row.case_values[2]) / count;
    row.average_sojourn_time = temporal_lazy_kahan_sum(row.case_values[3]) / count;
    row.average_service_time = temporal_lazy_kahan_sum(row.case_values[4]) / count;
    double resource_cases = 0;
    for (const auto& [resource, members_of] : cases_by_resource) resource_cases += static_cast<double>(members_of.size());
    row.avg_cases_per_resource = cases_by_resource.empty() ? 0.0 : resource_cases / static_cast<double>(cases_by_resource.size());
    row.avg_events_per_case = static_cast<double>(members.size()) / count;
    row.number_of_cases = static_cast<std::uint32_t>(order.size());
    row.avg_resources_per_case /= count;
    out.push_back(std::move(row));
  }
  return out;
}

}  // namespace minera
