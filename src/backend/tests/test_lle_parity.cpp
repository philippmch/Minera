// log_to_features.util.locally_linear_embedding against the public seeded
// bank docs/lle-bank.txt.gz (scripts/capture-lle-bank.py). smooth must return
// the reference's array bit for bit or raise its error. apply must raise the
// reference's error, or return its dates, feed the embedding the reference's
// feature matrix exactly, pick neighbours at the reference's distances (ties
// may go to other rows), and, given the reference's neighbours, compute its
// embedding: equal up to sign within 1e-7 when the eigenvalue is isolated,
// otherwise an eigenvector of the native matrix for the same eigenvalue.
// Smoothing the reference's embedding must give the reference's values bit
// for bit.
#include "minera/lle_reference.hpp"
#include "minera/timestamp.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
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
  std::size_t smooth = 0, smooth_raising = 0, apply = 0, apply_raising = 0, isolated = 0, degenerate = 0,
              tied = 0;
};

std::vector<double> numbers(const Json& array) {
  std::vector<double> out;
  for (const auto& item : array.items) out.push_back(item.number());
  return out;
}

void smooth_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto y = numbers(*item.source.find("y"));
  const auto box = item.source.find("box")->integer;
  const auto* raises = item.outcome.find("raises");
  try {
    const auto result = reference_lle_smooth(y, box);
    if (raises) failures.push_back(item.name + ": native returns where the reference raises");
    else if (result != numbers(*item.outcome.find("result"))) failures.push_back(item.name + ": values differ");
    else ++tally.smooth;
  } catch (const LleReferenceError& error) {
    if (!raises || std::string(error.what()) != raises->text + ": " + item.outcome.find("message")->text)
      failures.push_back(item.name + ": native raises " + error.what());
    else ++tally.smooth_raising;
  }
}

EventLog log_of(const Json& source) {
  EventLog log;
  for (const auto& events : source.find("traces")->items) {
    Trace trace;
    trace.case_id = "c" + std::to_string(log.traces.size());
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& event : events.items) {
      Event native{trace.case_id, event.items[0].text, format_timestamp(static_cast<double>(event.items[1].integer)), "",
                   trace.events.size()};
      native.activity_state = FirstClassAttributeState::kPresent;
      native.timestamp_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(native));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

LleNeighbors neighbors_of(const Json& array) {
  LleNeighbors out;
  for (const auto& row : array.items) {
    std::vector<std::size_t> indices;
    for (const auto& index : row.items) indices.push_back(static_cast<std::size_t>(index.integer));
    out.push_back(std::move(indices));
  }
  return out;
}

double squared_distance(const std::vector<double>& x, const std::vector<double>& y) {
  double total = 0.0;
  for (std::size_t k = 0; k < x.size(); ++k) total += (x[k] - y[k]) * (x[k] - y[k]);
  return total;
}

// Empty when the native embedding of the reference's neighbours matches.
std::string embedding_problem(const LleMatrix& rows, const LleNeighbors& neighbors, const std::vector<double>& expected,
                              Tally& tally) {
  const auto embedding = reference_lle_embedding(rows, neighbors);
  const auto& values = embedding.eigenvalues;
  const double scale = std::max(1.0, std::abs(values.back()));
  const double gap = std::min(values[1] - values[0], values.size() > 2 ? values[2] - values[1] : scale);
  if (gap > 1e-9 * scale) {
    double plus = 0.0, minus = 0.0;
    for (std::size_t k = 0; k < expected.size(); ++k) {
      plus = std::max(plus, std::abs(embedding.values[k] - expected[k]));
      minus = std::max(minus, std::abs(embedding.values[k] + expected[k]));
    }
    if (std::min(plus, minus) > 1e-7) return "embedding differs by " + std::to_string(std::min(plus, minus));
    ++tally.isolated;
    return {};
  }
  double residual = 0.0, norm = 0.0;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    double product = 0.0;
    for (std::size_t j = 0; j < expected.size(); ++j) product += embedding.m[i][j] * expected[j];
    residual = std::max(residual, std::abs(product - values[1] * expected[i]));
    norm += expected[i] * expected[i];
  }
  if (residual > 1e-8 * scale || std::abs(norm - 1.0) > 1e-9) return "reference vector is no eigenvector";
  ++tally.degenerate;
  return {};
}

void apply_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto log = log_of(item.source);
  const auto& outcome = item.outcome;
  const auto* raises = outcome.find("raises");
  if (raises) {
    try {
      (void)reference_lle_apply(log);
      failures.push_back(item.name + ": native returns where the reference raises");
    } catch (const LleReferenceError& error) {
      if (std::string(error.what()) != raises->text + ": " + outcome.find("message")->text)
        failures.push_back(item.name + ": native raises " + error.what());
      else ++tally.apply_raising;
    }
    return;
  }
  const auto features = reference_lle_features(log);
  std::vector<std::int64_t> dates;
  for (const auto& date : outcome.find("dates")->items) dates.push_back(date.integer * 1'000'000);
  if (features.dates != dates) return failures.push_back(item.name + ": dates differ");
  LleMatrix expected_rows;
  for (const auto& row : outcome.find("X")->items) expected_rows.push_back(numbers(row));
  if (features.rows != expected_rows) return failures.push_back(item.name + ": feature matrix differs");
  const auto reference = neighbors_of(*outcome.find("neighbors"));
  const auto native = reference_lle_neighbors(features.rows);
  bool tied = false;
  for (std::size_t i = 0; i < native.size(); ++i) {
    std::vector<double> mine, theirs;
    for (std::size_t k = 0; k < native[i].size(); ++k) {
      mine.push_back(squared_distance(features.rows[i], features.rows[native[i][k]]));
      theirs.push_back(squared_distance(features.rows[i], features.rows[reference.at(i).at(k)]));
    }
    std::sort(theirs.begin(), theirs.end());
    if (mine != theirs) return failures.push_back(item.name + ": neighbour distances differ in row " + std::to_string(i));
    tied = tied || native[i] != reference[i];
  }
  tally.tied += tied;
  const auto expected = numbers(*outcome.find("embedding"));
  const auto isolated_before = tally.isolated;
  if (auto problem = embedding_problem(features.rows, reference, expected, tally); !problem.empty())
    return failures.push_back(item.name + ": " + problem);
  const auto y = numbers(*outcome.find("y"));
  if (reference_lle_smooth(expected, reference_lle_box(features.rows.size())) != y)
    return failures.push_back(item.name + ": smoothing the reference embedding differs");
  const auto result = reference_lle_apply(log, "concept:name", [&](const LleMatrix&) { return reference; });
  double plus = 0.0, minus = 0.0;
  for (std::size_t k = 0; k < y.size(); ++k) {
    plus = std::max(plus, std::abs(result.values.at(k) - y[k]));
    minus = std::max(minus, std::abs(result.values.at(k) + y[k]));
  }
  if (result.dates != dates) return failures.push_back(item.name + ": apply dates differ");
  if (tally.isolated != isolated_before && std::min(plus, minus) > 1e-7)
    return failures.push_back(item.name + ": apply values differ");
  ++tally.apply;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: minera_lle_parity_tests <bank>\n";
    return 2;
  }
  std::vector<std::string> failures;
  Tally tally;
  for (const auto& item : read_bank(argv[1])) {
    try {
      if (item.kind == "smooth") smooth_case(item, tally, failures);
      else if (item.kind == "apply") apply_case(item, tally, failures);
      else failures.push_back(item.name + ": unknown kind " + item.kind);
    } catch (const std::exception& error) {
      failures.push_back(item.name + ": " + error.what());
    }
  }
  for (std::size_t k = 0; k < failures.size() && k < 30; ++k) std::cerr << failures[k] << "\n";
  std::cout << "lle: smooth " << tally.smooth << " arrays, " << tally.smooth_raising << " raising; apply " << tally.apply
            << " embeddings (" << tally.isolated << " isolated, " << tally.degenerate << " in a repeated eigenvalue, "
            << tally.tied << " with neighbour ties), " << tally.apply_raising << " raising; " << failures.size()
            << " failing cases\n";
  return failures.empty() ? 0 : 1;
}
