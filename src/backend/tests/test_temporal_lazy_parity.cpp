// log_to_features.variants.temporal_lazy against the public seeded bank
// docs/temporal-lazy-bank.txt.gz (scripts/capture-temporal-lazy-bank.py).
// _freq_to_polars_duration must return the reference's string or raise its
// exception type with its message. apply must raise the reference's
// exception type or return its buckets: labels, counts and the exact ratios
// equal, and each of the five case-metric means equal to the reference's,
// or to the Kahan mean of the native case values in an order Polars could
// have summed them (every order is tried for up to eight cases; larger
// buckets accept a relative 1e-12 and are counted apart).
#include "minera/temporal_lazy_reference.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Json {
  enum Kind { kNull, kBool, kInt, kFloat, kText, kArray, kObject } kind = kNull;
  bool boolean = false;
  long long integer = 0;
  double real = 0;
  std::string text;
  std::vector<Json> items;
  std::vector<std::pair<std::string, Json>> members;
  [[nodiscard]] const Json* find(const std::string& key) const {
    for (const auto& [name, value] : members)
      if (name == key) return &value;
    return nullptr;
  }
  [[nodiscard]] double number() const { return kind == kInt ? static_cast<double>(integer) : real; }
};

void append_utf8(std::string& out, unsigned long codepoint) {
  if (codepoint < 0x80) {
    out += static_cast<char>(codepoint);
  } else if (codepoint < 0x800) {
    out += static_cast<char>(0xc0 | (codepoint >> 6));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  } else if (codepoint < 0x10000) {
    out += static_cast<char>(0xe0 | (codepoint >> 12));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (codepoint >> 18));
    out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  }
}

class JsonReader {
 public:
  explicit JsonReader(const char* text) : at_(text) {}
  Json value() {
    space();
    Json out;
    if (*at_ == '{') {
      out.kind = Json::kObject;
      ++at_;
      for (space(); *at_ != '}'; space()) {
        auto key = value().text;
        space();
        ++at_;  // ':'
        out.members.emplace_back(std::move(key), value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '[') {
      out.kind = Json::kArray;
      ++at_;
      for (space(); *at_ != ']'; space()) {
        out.items.push_back(value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '"') {
      out.kind = Json::kText;
      for (++at_; *at_ != '"'; ++at_) {
        if (*at_ != '\\') {
          out.text += *at_;
          continue;
        }
        ++at_;
        if (*at_ == 'u') {
          auto codepoint = std::strtoul(std::string(at_ + 1, 4).c_str(), nullptr, 16);
          at_ += 4;
          if (codepoint >= 0xd800 && codepoint < 0xdc00 && at_[1] == '\\' && at_[2] == 'u') {
            const auto low = std::strtoul(std::string(at_ + 3, 4).c_str(), nullptr, 16);
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
            at_ += 6;
          }
          append_utf8(out.text, codepoint);
        } else {
          out.text += *at_ == 'n' ? '\n' : *at_ == 't' ? '\t' : *at_ == 'r' ? '\r' : *at_ == 'b' ? '\b'
                    : *at_ == 'f' ? '\f' : *at_;
        }
      }
      ++at_;
    } else if (*at_ == 't' || *at_ == 'f') {
      out.kind = Json::kBool;
      out.boolean = *at_ == 't';
      at_ += out.boolean ? 4 : 5;
    } else if (*at_ == 'n') {
      at_ += 4;
    } else {
      const char* start = at_;
      char* end = nullptr;
      const double number = std::strtod(at_, &end);
      at_ = end;
      if (std::string(start, static_cast<const char*>(end)).find_first_of(".eE") == std::string::npos) {
        out.kind = Json::kInt;
        out.integer = std::strtoll(start, nullptr, 10);
      } else {
        out.kind = Json::kFloat;
        out.real = number;
      }
    }
    return out;
  }

 private:
  void space() { while (*at_ == ' ' || *at_ == '\n') ++at_; }
  const char* at_;
};

struct BankCase {
  std::string name, kind;
  Json source, outcome;
};

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 2) continue;
    auto value = [&] { return JsonReader(line.c_str() + 2).value(); };
    switch (line[0]) {
      case 'C': cases.emplace_back(); cases.back().name = line.substr(2); break;
      case 'K': cases.back().kind = line.substr(2); break;
      case 'S': cases.back().source = value(); break;
      case 'F': cases.back().outcome = value(); break;
      default: break;
    }
  }
  return cases;
}

struct Tally {
  std::size_t freq = 0, freq_raising = 0, apply = 0, apply_raising = 0, permuted = 0, approximate = 0;
};

void freq_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto* given = item.source.find("freq");
  const std::optional<std::string> freq =
      given->kind == Json::kText ? std::optional<std::string>(given->text) : std::nullopt;
  const auto* raises = item.outcome.find("raises");
  try {
    const auto result = reference_freq_to_polars_duration(freq);
    if (raises) failures.push_back(item.name + ": native returns " + result + " where the reference raises");
    else if (result != item.outcome.find("result")->text) failures.push_back(item.name + ": native returns " + result);
    else ++tally.freq;
  } catch (const TemporalLazyError& error) {
    if (!raises || error.type() != raises->text ||
        std::string(error.what()) != error.type() + ": " + item.outcome.find("message")->text)
      failures.push_back(item.name + ": native raises " + error.what());
    else ++tally.freq_raising;
  }
}

// Whether the reference mean is the Kahan mean of the case values in some
// order: 0 no, 1 exactly so, 2 only approximately (too many cases to try).
int reachable(std::vector<double> values, double expected) {
  const auto count = static_cast<double>(values.size());
  if (values.size() > 8) {
    const double native = temporal_lazy_kahan_sum(values) / count;
    return std::abs(native - expected) <= 1e-12 * std::max(1.0, std::abs(expected)) ? 2 : 0;
  }
  std::vector<std::size_t> order(values.size());
  std::iota(order.begin(), order.end(), 0);
  std::vector<double> permuted(values.size());
  do {
    for (std::size_t k = 0; k < order.size(); ++k) permuted[k] = values[order[k]];
    if (temporal_lazy_kahan_sum(permuted) / count == expected) return 1;
  } while (std::next_permutation(order.begin(), order.end()));
  return 0;
}

// 0 mismatch, 1 exact, 2 by another summation order, 3 approximately.
int compare(const std::vector<TemporalLazyRow>& rows, const Json& expected, std::string& problem) {
  const auto& items = expected.items;
  if (rows.size() != items.size()) {
    problem = "bucket count " + std::to_string(rows.size()) + " vs " + std::to_string(items.size());
    return 0;
  }
  int worst = 1;
  for (std::size_t b = 0; b < rows.size(); ++b) {
    const auto& row = rows[b];
    const auto& cells = items[b].items;
    const double exact[] = {static_cast<double>(row.unique_resources), static_cast<double>(row.unique_activities),
                            static_cast<double>(row.num_events), static_cast<double>(row.total_number_of_reworked_activities),
                            row.avg_cases_per_resource, row.avg_events_per_case,
                            static_cast<double>(row.number_of_cases), row.avg_resources_per_case};
    const std::size_t exact_at[] = {1, 2, 3, 9, 10, 11, 12, 13};
    if (row.timestamp != cells[0].integer) {
      problem = "bucket " + std::to_string(b) + " label differs";
      return 0;
    }
    for (std::size_t k = 0; k < 8; ++k)
      if (exact[k] != cells[exact_at[k]].number()) {
        problem = "bucket " + std::to_string(b) + " column " + std::to_string(exact_at[k]) + " differs";
        return 0;
      }
    const double means[] = {row.average_arrival_rate, row.average_finish_rate, row.average_waiting_time,
                            row.average_sojourn_time, row.average_service_time};
    for (std::size_t k = 0; k < 5; ++k) {
      const double reference = cells[4 + k].number();
      if (means[k] == reference) continue;
      const int how = reachable(row.case_values[k], reference);
      if (!how) {
        std::ostringstream message;
        message.precision(17);
        message << "bucket " << b << " mean " << 4 + k << " native " << means[k] << " reference " << reference;
        problem = message.str();
        return 0;
      }
      worst = std::max(worst, how + 1);
    }
  }
  return worst;
}

void apply_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto& source = item.source;
  TemporalLazyFrame frame;
  frame.utc = source.find("utc")->boolean;
  for (const auto& spec : source.find("columns")->items) {
    TemporalLazyColumn column;
    column.name = spec.find("name")->text;
    const auto& kind = spec.find("kind")->text;
    column.kind = kind == "time" ? TemporalLazyColumn::kTime : kind == "int" ? TemporalLazyColumn::kInt
                                                                             : TemporalLazyColumn::kText;
    for (const auto& cell : spec.find("cells")->items) {
      if (cell.kind == Json::kNull) column.cells.emplace_back(std::monostate{});
      else if (cell.kind == Json::kInt) column.cells.emplace_back(static_cast<std::int64_t>(cell.integer));
      else column.cells.emplace_back(cell.text);
    }
    frame.columns.push_back(std::move(column));
  }
  TemporalLazyParameters parameters;
  for (const auto& [key, value] : source.find("params")->members) {
    if (key == "pm4py:param:case_id_key") parameters.case_id_key = value.text;
    else if (key == "pm4py:param:activity_key") parameters.activity_key = value.text;
    else if (key == "pm4py:param:resource_key") parameters.resource_key = value.text;
    else if (key == "pm4py:param:timestamp_key") parameters.timestamp_key = value.text;
    else if (key == "pm4py:param:start_timestamp_key") parameters.start_timestamp_key = value.text;
    else if (key == "grouper_freq")
      parameters.grouper_freq = value.kind == Json::kText ? std::optional<std::string>(value.text) : std::nullopt;
  }
  const auto& observed = item.outcome.find("observed")->items;
  std::vector<TemporalLazyRow> rows;
  std::string raised;
  try {
    rows = reference_temporal_lazy(frame, parameters);
  } catch (const TemporalLazyError& error) {
    raised = error.type();
  }
  std::string problem;
  for (const auto& outcome : observed) {
    if (const auto* raises = outcome.find("raises")) {
      if (raised == raises->text) {
        ++tally.apply_raising;
        return;
      }
      problem = "reference raises " + raises->text;
      continue;
    }
    if (!raised.empty()) {
      problem = "native raises " + raised;
      continue;
    }
    std::string detail;
    const int how = compare(rows, *outcome.find("rows"), detail);
    if (how) {
      ++tally.apply;
      if (how == 2) ++tally.permuted;
      if (how == 3) ++tally.approximate;
      return;
    }
    problem = detail;
  }
  failures.push_back(item.name + ": " + problem);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: minera_temporal_lazy_parity_tests <bank>\n";
    return 2;
  }
  std::vector<std::string> failures;
  Tally tally;
  for (const auto& item : read_bank(argv[1])) {
    try {
      if (item.kind == "freq") freq_case(item, tally, failures);
      else if (item.kind == "apply") apply_case(item, tally, failures);
      else failures.push_back(item.name + ": unknown kind " + item.kind);
    } catch (const std::exception& error) {
      failures.push_back(item.name + ": " + error.what());
    }
  }
  for (std::size_t k = 0; k < failures.size() && k < 30; ++k) std::cerr << failures[k] << "\n";
  std::cout << "temporal_lazy: freq " << tally.freq << " results, " << tally.freq_raising << " raising; apply "
            << tally.apply << " tables (" << tally.permuted << " with means in another summation order, "
            << tally.approximate << " approximate), " << tally.apply_raising << " raising; " << failures.size()
            << " failing cases\n";
  return failures.empty() ? 0 : 1;
}
