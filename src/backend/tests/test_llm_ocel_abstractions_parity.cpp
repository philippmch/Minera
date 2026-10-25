// OCEL LLM abstractions against the public capture
// (docs/llm-ocel-abstractions-bank.json.gz, written to
// src/backend/tests/fixtures/llm_ocel_abstractions_bank.txt). Seeded OCELs go
// through the public OCDFG description under default, header, performance,
// budget and uncomputed-performance options, and through the object-feature
// description of every object type; every text must match byte for byte, or
// the call must raise.
#include "minera/llm_abstractions.hpp"
#include "minera/ocel.hpp"

#include <cstdio>
#include <exception>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::string part;
  std::istringstream stream(text);
  while (std::getline(stream, part, separator)) parts.push_back(part);
  return parts;
}

std::string unescape(const std::string& text) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 1 < text.size()) {
      out += text[++i] == 'n' ? '\n' : text[i];
    } else {
      out += text[i];
    }
  }
  return out;
}

struct Call {
  std::string name;
  std::optional<std::string> error;
  std::string observed;
};

struct Case {
  std::string name;
  Ocel log;
  std::vector<Call> calls;
};

void add_event(Ocel& log, const std::vector<std::string>& fields) {
  const int minutes = std::stoi(fields.at(3));
  char stamp[64];
  std::snprintf(stamp, sizeof stamp, "2024-01-01T%02d:%02d:00", minutes / 60, minutes % 60);
  log.events.push_back({fields.at(1), fields.at(2), stamp, {}});
  if (fields.at(4) == "-") return;
  for (const auto& object : split(fields[4], ',')) log.relations.push_back({fields[1], object, ""});
}

std::vector<Case> load(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  std::vector<Case> cases;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto tag = line.substr(0, 1);
    const auto rest = line.size() > 2 ? line.substr(2) : std::string();
    if (tag == "C") {
      cases.push_back({rest, {}, {}});
    } else if (tag == "B") {
      const auto fields = split(line, ' ');
      cases.back().log.objects.push_back({fields.at(1), fields.at(2), {}});
    } else if (tag == "E") {
      add_event(cases.back().log, split(line, ' '));
    } else if (tag == "K") {
      cases.back().calls.push_back({rest, std::nullopt, {}});
    } else if (tag == "X") {
      cases.back().calls.back().error = rest;
    } else if (tag == "O") {
      cases.back().calls.back().observed = unescape(rest);
    }
  }
  return cases;
}

LlmAbstractionOptions options_for(const std::string& call) {
  LlmAbstractionOptions options;
  if (call == "ocdfg-noheader") options.include_header = false;
  if (call == "ocdfg-noperformance") options.include_performance = false;
  if (call == "ocdfg-short") options.max_len = 400;
  if (call == "ocdfg-uncomputed") options.include_performance = false;
  if (call == "ocdfg-uncomputed" || call == "ocdfg-uncomputed-performance") options.compute_edges_performance = false;
  if (call == "features-short") {
    options.include_header = false;
    options.max_len = 600;
  }
  return options;
}

// Empty when the native text matches the observation, otherwise a description.
template <typename Describe>
std::string text_case(const Call& call, Describe describe) {
  std::string text;
  try {
    text = describe();
  } catch (const std::exception& error) {
    return call.error ? "" : std::string("raised ") + error.what();
  }
  if (call.error) return "expected " + *call.error;
  if (text == call.observed) return "";
  const auto& expected = call.observed;
  std::size_t at = 0;
  while (at < expected.size() && at < text.size() && expected[at] == text[at]) ++at;
  return "text differs at byte " + std::to_string(at) + ": expected [" + expected.substr(at, 80) + "] got [" +
         text.substr(at, 80) + "]";
}

std::string ocel_ocdfg_case(const Ocel& log, const Call& call) {
  return text_case(call, [&] { return llm_ocel_ocdfg_description(log, options_for(call.name)); });
}

std::string ocel_features_case(const Ocel& log, const Call& call) {
  const auto slash = call.name.find('/');
  return text_case(call, [&] {
    return llm_ocel_features_description(log, call.name.substr(slash + 1), options_for(call.name.substr(0, slash)));
  });
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/llm_ocel_abstractions_bank.txt";
  int failures = 0;
  std::size_t compared = 0, errors = 0;
  std::map<std::string, std::size_t> failed_by_call;
  for (const auto& item : load(path)) {
    for (const auto& call : item.calls) {
      ++compared;
      if (call.error) ++errors;
      const auto family = call.name.substr(0, call.name.find_first_of("-/"));
      std::string detail;
      try {
        detail = family == "ocdfg" ? ocel_ocdfg_case(item.log, call) : ocel_features_case(item.log, call);
      } catch (const std::exception& error) {
        detail = std::string("threw ") + error.what();
      }
      if (!detail.empty()) {
        ++failures;
        ++failed_by_call[call.name.substr(0, call.name.find('/'))];
        if (failures <= 30)
          std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), call.name.c_str(), detail.c_str());
      }
    }
  }
  for (const auto& [call, count] : failed_by_call) std::fprintf(stderr, "%s: %zu failing\n", call.c_str(), count);
  std::printf("%zu calls compared (%zu public errors)\n", compared, errors);
  if (compared < 1700 || errors < 400) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
