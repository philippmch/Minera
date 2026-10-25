// algo.anonymization.trace_variant_query against the public seeded bank
// (docs/trace-variant-query-bank.txt.gz,
// scripts/capture-trace-variant-query-bank.py): every exponential-mechanism
// draw, behavioural-appropriateness relation and violation count, generated
// log, SaCoFa query result (cases and their traces) and error (class and
// message) must equal the reference's.
#include "minera/trace_variant_query_reference.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> out;
  std::string piece;
  std::istringstream stream(text);
  while (std::getline(stream, piece, separator))
    if (!piece.empty()) out.push_back(piece);
  return out;
}

std::vector<std::string> labels(const std::string& word) {
  std::vector<std::string> out;
  if (word == "-") return out;
  for (const char c : word) out.emplace_back(1, c == '$' ? '\0' : c);
  for (auto& label : out)
    if (label[0] == '\0') label = kTraceVariantQueryTraceEnd;
  return out;
}

std::vector<std::vector<std::string>> traces(const std::string& text) {
  std::vector<std::vector<std::string>> out;
  if (text == ".") return out;
  for (const auto& word : split(text, ',')) out.push_back(labels(word));
  return out;
}

// The bank's " | "-separated fields of a line after its tag.
std::vector<std::string> fields(const std::string& line) {
  std::vector<std::string> out;
  std::size_t start = 2;
  for (;;) {
    const auto at = line.find(" |", start);
    out.push_back(line.substr(start, at == std::string::npos ? std::string::npos : at - start));
    if (at == std::string::npos) return out;
    start = at + 2;
    while (start < line.size() && line[start] == ' ') ++start;
  }
}

std::string cases_text(const std::vector<TraceVariantQueryReferenceCase>& cases) {
  std::string out;
  for (const auto& c : cases) {
    if (!out.empty()) out += ' ';
    out += std::to_string(c.case_id) + '=';
    for (const auto& event : c.events) out += event;
    if (c.events.empty()) out += '-';
  }
  return out;
}

template <typename Call>
std::string outcome(Call&& call) {
  try {
    return cases_text(call());
  } catch (const TraceVariantQueryReferenceError& error) {
    return std::string("! ") + error.type + ' ' + error.what();
  }
}

void report(const std::string& line, const std::string& expected, const std::string& actual, std::size_t& failures) {
  if (expected != actual && ++failures <= 20)
    std::cerr << line.substr(0, 300) << "\n  expected " << expected.substr(0, 300) << "\n  got      "
              << actual.substr(0, 300) << '\n';
}

void check_exp_mech(const std::string& line, std::size_t& failures) {
  std::istringstream in(line.substr(2));
  long n = 0, expected = 0;
  std::string epsilon;
  std::uint64_t seed = 0;
  in >> n >> epsilon >> seed >> expected;
  const auto actual = reference_exp_mech(n, std::strtod(epsilon.c_str(), nullptr), static_cast<std::uint32_t>(seed));
  report(line, std::to_string(expected), std::to_string(actual), failures);
}

void check_relations(const std::string& line, std::size_t& failures) {
  const auto parts = fields(line);
  const auto log = traces(parts.at(0));
  std::vector<std::string> events;
  for (const auto& trace : log)
    for (const auto& event : trace)
      if (std::find(events.begin(), events.end(), event) == events.end()) events.push_back(event);
  const auto relations = reference_ba_relations(log, events);
  std::string follows, precedes;
  for (const auto& x : events)
    for (const auto& y : events) {
      follows += relations.follows.at({x, y});
      precedes += relations.precedes.at({x, y});
    }
  report(line, parts.at(1), follows.empty() ? "." : follows, failures);
  report(line, parts.at(2), precedes.empty() ? "." : precedes, failures);
  std::string expected, actual;
  if (parts.size() > 3) {
    for (const auto& pair : split(parts[3], ' ')) {
      const auto at = pair.find('=');
      expected += pair + ' ';
      actual += pair.substr(0, at) + '=' + std::to_string(reference_ba_violations(relations, labels(pair.substr(0, at)))) + ' ';
    }
  }
  report(line, expected, actual, failures);
}

void check_generate(const std::string& line, std::size_t& failures) {
  const auto parts = fields(line);
  std::vector<std::pair<std::string, long>> frequencies;
  if (parts.at(0) != ".") {
    for (const auto& pair : split(parts[0], ' ')) {
      const auto at = pair.rfind('=');
      frequencies.emplace_back(pair.substr(0, at), std::stol(pair.substr(at + 1)));
    }
  }
  report(line, parts.size() > 1 ? parts[1] : "", cases_text(reference_generate_variant_log(frequencies)), failures);
}

struct Query {
  std::vector<std::string> parts;
  std::vector<std::vector<std::string>> traces;
  TraceVariantQueryReferenceOptions options;
};

Query query(const std::string& line) {
  Query out;
  out.parts = fields(line);
  std::istringstream in(out.parts.at(0));
  std::string log, epsilon;
  std::uint64_t seed = 0;
  in >> log >> epsilon >> out.options.k >> out.options.p >> seed;
  out.options.epsilon = std::strtod(epsilon.c_str(), nullptr);
  out.options.seed = static_cast<std::uint32_t>(seed);
  out.traces = traces(log);
  return out;
}

void check_sacofa(const std::string& line, std::size_t& failures) {
  const auto q = query(line);
  const auto actual = outcome([&] { return reference_sacofa_trace_variant_query(q.traces, q.options); });
  report(line, q.parts.size() > 1 ? q.parts[1] : "", actual, failures);
}

void check_laplace(const std::string& line, std::size_t& failures) {
  const auto q = query(line);
  const auto actual = outcome([&]() -> std::vector<TraceVariantQueryReferenceCase> {
    reference_laplace_trace_variant_query(q.traces, q.options);
  });
  report(line, q.parts.size() > 1 ? q.parts[1] : "", actual, failures);
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + argv[1]);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::size_t calls = 0, failures = 0;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 3) continue;
    ++calls;
    switch (line[0]) {
      case 'E': check_exp_mech(line, failures); break;
      case 'B': check_relations(line, failures); break;
      case 'G': check_generate(line, failures); break;
      case 'S': check_sacofa(line, failures); break;
      default: check_laplace(line, failures); break;
    }
  }
  std::cout << calls << " trace_variant_query calls compared, " << failures << " failures\n";
  return failures || calls == 0 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
