// algo.transformation.to_embeddings against the public seeded bank
// docs/embedding-bank.txt.gz (scripts/capture-embedding-bank.py): with the
// encoder replaying the stub model's recorded answers, every function must
// make the same encode calls (model and sentences) and return the same
// vectors, similarities (value and dtype), identifiers and kept rows, or
// raise the same Python exception type.
#include "minera/embedding_reference.hpp"

#include "minera/connector_reference.hpp"

#include "../gzip_codec.hpp"

#include <cstdlib>
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
  std::map<std::string, std::size_t> results, raising;
};

double number(const Json& value) { return value.kind == Json::kInt ? static_cast<double>(value.integer) : value.real; }

EmbeddingReferenceVector vector_of(const Json& value) {
  EmbeddingReferenceVector out;
  const Json* items = &value;
  if (value.kind == Json::kObject) {
    items = value.find("f32");
    out.float32 = true;
  }
  for (const auto& item : items->items) out.values.push_back(number(item));
  return out;
}

EmbeddingReferenceValue value_of(const Json& value) {
  if (value.kind == Json::kInt) return value.integer;
  return value.text;
}

EmbeddingReferenceLog log_of(const Json& value) {
  EmbeddingReferenceLog log;
  for (const auto& [name, column] : value.members) {
    log.columns.push_back(name);
    if (log.rows.size() < column.items.size()) log.rows.resize(column.items.size());
    for (std::size_t r = 0; r < column.items.size(); ++r) log.rows[r].push_back(value_of(column.items[r]));
  }
  return log;
}

EmbeddingReferenceParameters parameters_of(const Json& value) {
  EmbeddingReferenceParameters out;
  if (const auto* model = value.find("embedding_model")) out.model = model->text;
  if (const auto* key = value.find("pm4py:param:attribute_key")) out.attribute_key = key->text;
  if (const auto* key = value.find("event_id_key")) out.event_id_key = key->text;
  if (const auto* keep = value.find("keep_cases")) out.keep_cases = keep->boolean;
  return out;
}

Json json_of(const EmbeddingReferenceValue& value) {
  Json out;
  if (std::holds_alternative<std::int64_t>(value)) {
    out.kind = Json::kInt;
    out.integer = std::get<std::int64_t>(value);
  } else {
    out.kind = Json::kText;
    out.text = std::get<std::string>(value);
  }
  return out;
}

bool same_vectors(const std::vector<std::vector<float>>& mine, const Json& expected) {
  if (mine.size() != expected.items.size()) return false;
  for (std::size_t k = 0; k < mine.size(); ++k) {
    if (mine[k].size() != expected.items[k].items.size()) return false;
    for (std::size_t j = 0; j < mine[k].size(); ++j)
      if (static_cast<double>(mine[k][j]) != number(expected.items[k].items[j])) return false;
  }
  return true;
}

// Replays the recorded encode calls around compare(encode, parameters),
// which runs the function and says whether its result matches; returns the
// problem, empty when everything matches.
template <typename Compare>
std::string replay(const BankCase& item, Compare compare) {
  const auto& encodes = item.outcome.find("encodes")->items;
  std::size_t next = 0;
  std::string problem;
  const EmbeddingReferenceEncode encode = [&](const std::string& model, const std::vector<std::string>& sentences) {
    std::vector<std::vector<float>> out;
    if (next >= encodes.size()) {
      problem = "an extra encode call";
      for (std::size_t k = 0; k < sentences.size(); ++k) out.emplace_back(4, 0.0F);
      return out;
    }
    const auto& call = encodes[next++];
    std::vector<std::string> expected;
    for (const auto& s : call.items[1].items) expected.push_back(s.text);
    if (problem.empty() && (call.items[0].text != model || expected != sentences))
      problem = "encode call " + std::to_string(next - 1) + " differs";
    for (const auto& v : call.items[2].items) {
      out.emplace_back();
      for (const auto& x : v.items) out.back().push_back(static_cast<float>(number(x)));
    }
    return out;
  };
  const bool raises = item.outcome.find("raises") != nullptr;
  try {
    const bool same = compare(encode, parameters_of(*item.source.find("params")));
    if (raises) return "native returns where the reference raises " + item.outcome.find("raises")->text;
    if (!same) return "result differs";
  } catch (const ConnectorReferenceError& error) {
    if (!raises || error.type() != item.outcome.find("raises")->text) return std::string("native raises ") + error.what();
  }
  if (!problem.empty()) return problem;
  if (next != encodes.size()) return "fewer encode calls";
  return "";
}

void record(const BankCase& item, const std::string& problem, Tally& tally, std::vector<std::string>& failures) {
  if (!problem.empty()) failures.push_back(item.name + ": " + problem);
  else ++(item.outcome.find("raises") ? tally.raising : tally.results)[item.kind];
}

const Json& result_of(const BankCase& item) {
  static const Json none;
  const auto* result = item.outcome.find("result");
  return result ? *result : none;
}

void embed_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  record(item, replay(item, [&](const EmbeddingReferenceEncode& encode, const EmbeddingReferenceParameters& p) {
           const auto mine = reference_embed_sentence(item.source.find("sentence")->text, p.model, encode);
           Json expected;
           expected.kind = Json::kArray;
           expected.items = {result_of(item)};
           return same_vectors({mine}, expected);
         }), tally, failures);
}

void similarity_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  record(item, replay(item, [&](const EmbeddingReferenceEncode& encode, const EmbeddingReferenceParameters& p) {
           const auto& target = *item.source.find("target");
           std::variant<std::string, EmbeddingReferenceVector> query;
           if (target.kind == Json::kText) query = target.text;
           else query = vector_of(target);
           std::vector<EmbeddingReferenceVector> embeddings;
           for (const auto& e : item.source.find("embeddings")->items) embeddings.push_back(vector_of(e));
           const auto mine = reference_embedding_similarity(query, embeddings, p.model, encode);
           const auto& expected = result_of(item).items;
           bool same = mine.size() == expected.size();
           for (std::size_t k = 0; same && k < mine.size(); ++k)
             same = mine[k].value == number(expected[k].items[0]) &&
                    mine[k].float32 == (expected[k].items[1].text == "float32");
           return same;
         }), tally, failures);
}

bool same_embeddings(const EmbeddingReferenceEmbeddings& mine, const Json& expected) {
  bool same = same_vectors(mine.embeddings, expected.items[1]) &&
              mine.identifiers.size() == expected.items[0].items.size();
  for (std::size_t k = 0; same && k < mine.identifiers.size(); ++k) same = json_of(mine.identifiers[k]) == expected.items[0].items[k];
  return same;
}

bool same_rows(const std::vector<std::size_t>& mine, const Json& expected) {
  bool same = mine.size() == expected.items.size();
  for (std::size_t r = 0; same && r < mine.size(); ++r) same = static_cast<std::int64_t>(mine[r]) == expected.items[r].integer;
  return same;
}

std::optional<EmbeddingReferenceGiven> given_of(const BankCase& item) {
  std::optional<EmbeddingReferenceGiven> given;
  if (const auto* g = item.source.find("given"); g && g->kind == Json::kArray) {
    given.emplace();
    for (const auto& id : g->items[0].items) given->identifiers.push_back(value_of(id));
    for (const auto& e : g->items[1].items) given->embeddings.push_back(vector_of(e));
  }
  return given;
}

// cases_transformers: apply and keep_top_k_per_similarity.
void cases_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  record(item, replay(item, [&](const EmbeddingReferenceEncode& encode, const EmbeddingReferenceParameters& p) {
           const auto log = log_of(*item.source.find("log"));
           if (item.kind == "cases") return same_embeddings(reference_case_embeddings(log, p, encode), result_of(item));
           return same_rows(reference_keep_top_k_cases(log, item.source.find("target")->text,
                                                       item.source.find("k")->integer, given_of(item), p, encode),
                            result_of(item));
         }), tally, failures);
}

// events_transformers: apply and keep_top_k_per_similarity.
void events_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  record(item, replay(item, [&](const EmbeddingReferenceEncode& encode, const EmbeddingReferenceParameters& p) {
           const auto log = log_of(*item.source.find("log"));
           if (item.kind == "events") return same_embeddings(reference_event_embeddings(log, p, encode), result_of(item));
           return same_rows(reference_keep_top_k_events(log, item.source.find("target")->text,
                                                        item.source.find("k")->integer, given_of(item), p, encode),
                            result_of(item));
         }), tally, failures);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: minera_embedding_parity_tests <bank>\n";
    return 2;
  }
  std::vector<std::string> failures;
  Tally tally;
  for (const auto& item : read_bank(argv[1])) {
    try {
      if (item.kind == "embed") embed_case(item, tally, failures);
      else if (item.kind == "similarity") similarity_case(item, tally, failures);
      else if (item.kind == "cases" || item.kind == "top_cases") cases_case(item, tally, failures);
      else if (item.kind == "events" || item.kind == "top_events") events_case(item, tally, failures);
      else failures.push_back(item.name + ": unknown function " + item.kind);
    } catch (const std::exception& error) {
      failures.push_back(item.name + ": " + error.what());
    }
  }
  for (std::size_t k = 0; k < failures.size() && k < 30; ++k) std::cerr << failures[k] << "\n";
  std::cout << "embeddings:";
  for (const auto& [kind, count] : tally.results) std::cout << " " << kind << " " << count << ", " << tally.raising[kind] << " raising;";
  std::cout << " " << failures.size() << " failing cases\n";
  return failures.empty() ? 0 : 1;
}
