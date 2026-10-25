// algo.querying.llm.connectors against the public seeded bank
// docs/llm-connector-bank.txt.gz (scripts/capture-llm-bank.py): every
// connector, given the same prompt, parameters and stubbed response, must
// send the reference's POST (url, headers in order, payload with its keys in
// order, timeout) and return the reference's answer, or raise the same
// Python exception type (and, for an API error, the same message).
#include "minera/llm_connector_reference.hpp"

#include "../gzip_codec.hpp"

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;
using Json = ConnectorReferenceJson;

// JSON with members in document order, ints and floats apart.
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
        if (*at_ == '\\') {
          ++at_;
          out.text += *at_ == 'n' ? '\n' : *at_ == 't' ? '\t' : *at_;
          continue;
        }
        out.text += *at_;
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
  std::map<std::string, std::size_t> answers, raising;
};

template <typename Connector>
void query_case(const BankCase& item, const std::filesystem::path& directory, Tally& tally,
                std::vector<std::string>& failures, Connector connector) {
  const auto& source = item.source;
  // The images the reference read, written where image_path points.
  Json parameters = *source.find("params");
  for (auto& [key, value] : parameters.members)
    if (key == "image_path" && value.kind == Json::kText) value.text = (directory / value.text).string();
  for (const auto& [name, content] : source.find("images")->members)
    std::ofstream(directory / name, std::ios::binary) << content.text;
  std::vector<LlmReferenceRequest> requests;
  const LlmReferencePost post = [&](const LlmReferenceRequest& request) {
    requests.push_back(request);
    return *source.find("response");
  };
  std::string problem;
  const auto* raises = item.outcome.find("raises");
  try {
    const auto answer = connector(source.find("prompt")->text, parameters, post);
    if (raises) problem = "native returns where the reference raises " + raises->text;
    else if (!(answer == *item.outcome.find("result"))) problem = "answer differs";
  } catch (const ConnectorReferenceError& error) {
    if (!raises || error.type() != raises->text) problem = std::string("native raises ") + error.what();
    else if (error.type() == "Exception" && error.what() != "Exception: " + item.outcome.find("message")->text)
      problem = std::string("message differs: ") + error.what();
  }
  const auto& expected = item.outcome.find("calls")->items;
  if (problem.empty() && requests.size() != expected.size()) problem = "POST count differs";
  for (std::size_t k = 0; problem.empty() && k < requests.size(); ++k) {
    const auto& call = expected[k];
    const auto& r = requests[k];
    Json headers;
    headers.kind = Json::kObject;
    headers.members = r.headers;
    const auto* timeout = call.find("timeout");
    if (r.url != call.find("url")->text) problem = "url differs: " + r.url;
    else if (!(headers == *call.find("headers"))) problem = "headers differ";
    else if (!(r.payload == *call.find("json"))) problem = "payload differs";
    else if (timeout ? !r.timeout || *r.timeout != timeout->integer : r.timeout.has_value()) problem = "timeout differs";
  }
  for (const auto& [name, content] : source.find("images")->members) std::filesystem::remove(directory / name);
  if (!problem.empty()) failures.push_back(item.name + ": " + problem);
  else ++(raises ? tally.raising : tally.answers)[item.kind];
}

void anthropic_case(const BankCase& item, const std::filesystem::path& directory, Tally& tally,
                    std::vector<std::string>& failures) {
  query_case(item, directory, tally, failures, reference_anthropic_query);
}

void openai_case(const BankCase& item, const std::filesystem::path& directory, Tally& tally,
                 std::vector<std::string>& failures) {
  query_case(item, directory, tally, failures, reference_openai_query);
}

void google_case(const BankCase& item, const std::filesystem::path& directory, Tally& tally,
                 std::vector<std::string>& failures) {
  query_case(item, directory, tally, failures, reference_google_query);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: minera_llm_connector_parity_tests <bank>\n";
    return 2;
  }
  const auto directory = std::filesystem::temp_directory_path() / ("minera-llm-parity-" + std::to_string(::getpid()));
  std::filesystem::create_directories(directory);
  std::vector<std::string> failures;
  Tally tally;
  for (const auto& item : read_bank(argv[1])) {
    try {
      if (item.kind == "anthropic") anthropic_case(item, directory, tally, failures);
      else if (item.kind == "openai") openai_case(item, directory, tally, failures);
      else if (item.kind == "google") google_case(item, directory, tally, failures);
      else failures.push_back(item.name + ": unknown connector " + item.kind);
    } catch (const std::exception& error) {
      failures.push_back(item.name + ": " + error.what());
    }
  }
  std::filesystem::remove_all(directory);
  for (std::size_t k = 0; k < failures.size() && k < 30; ++k) std::cerr << failures[k] << "\n";
  std::cout << "llm connectors:";
  for (const auto& [kind, count] : tally.answers)
    std::cout << " " << kind << " " << count << " answers, " << tally.raising[kind] << " raising;";
  std::cout << " " << failures.size() << " failing cases\n";
  return failures.empty() ? 0 : 1;
}
