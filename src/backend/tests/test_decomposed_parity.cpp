// algo.conformance.alignments.decomposed.variants.recompos_maximal and
// objects.petri_net.utils.decomposition against the public seeded bank
// docs/decomposed-bank.txt.gz (scripts/capture-decomposed-bank.py):
// - decompose must give the reference's components exactly, in order, and
//   every merge_sublist_nets call the reference's merged component;
// - the best-worst cost must be the visible model moves of the reference's
//   empty-trace alignment of the whole net, at the same optimal cost;
// - every alignment the reference's align returns must be optimal: the
//   native Dijkstra alignment has the same cost (which optimal alignment
//   comes back varies with object addresses there, so identical ones are
//   tallied);
// - replaying the reference's component alignments, apply must make the
//   reference's align, merge and recompose calls in order and give its cost,
//   alignment, fitness and bwc for every trace, and must raise where the
//   reference never returns;
// - the fully native apply must give well-formed results whose cost follows
//   from the alignment (another optimal component alignment can change the
//   merges, so equal costs are tallied).
#include "minera/decomposed_reference.hpp"

#include "../gzip_codec.hpp"

#include <cmath>
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

struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  double number = 0;
  bool boolean = false;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  [[nodiscard]] const Json* get(const std::string& key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? nullptr : &found->second;
  }
};

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
        space(); ++at_;  // ':'
        out.fields.emplace(std::move(key), value());
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
      out.kind = Json::kString;
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
      out.kind = Json::kNumber;
      char* end = nullptr;
      out.number = std::strtod(at_, &end);
      at_ = end;
    }
    return out;
  }

 private:
  void space() { while (*at_ == ' ' || *at_ == '\n') ++at_; }
  const char* at_;
};


struct BankCase {
  std::string name;
  Json head, net, components, bwc, apply;
  std::vector<Json> variants;
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
      case 'T': cases.back().head = value(); break;
      case 'N': cases.back().net = value(); break;
      case 'D': cases.back().components = value(); break;
      case 'B': cases.back().bwc = value(); break;
      case 'V': cases.back().variants.push_back(value()); break;
      case 'P': cases.back().apply = value(); break;
      default: break;
    }
  }
  return cases;
}

std::vector<std::string> strings(const Json& value) {
  std::vector<std::string> out;
  for (const auto& item : value.items) out.push_back(item.text);
  return out;
}

std::vector<std::pair<std::string, std::size_t>> marking_of(const Json& value) {
  std::vector<std::pair<std::string, std::size_t>> out;
  for (const auto& entry : value.items) out.emplace_back(entry.items[0].text, static_cast<std::size_t>(entry.items[1].number));
  return out;
}

DecomposedReferenceNet net_of(const Json& value) {
  DecomposedReferenceNet net;
  net.places = strings(value.items[0]);
  for (const auto& t : value.items[1].items)
    net.transitions.emplace_back(t.items[0].text, t.items[1].kind == Json::kString ? std::optional(t.items[1].text) : std::nullopt);
  for (const auto& a : value.items[2].items) net.arcs.emplace_back(a.items[0].text, a.items[1].text);
  net.initial = marking_of(value.items[3]);
  net.final = marking_of(value.items[4]);
  return net;
}

DecomposedReferenceComponent component_of(const Json& value) {
  return {net_of(value), strings(value.items[5])};
}

DecomposedReferenceMoves moves_of(const Json& value) {
  DecomposedReferenceMoves out;
  for (const auto& move : value.items) {
    DecomposedReferenceMove m{move.items[0].items[0].text, move.items[0].items[1].text, move.items[1].items[0].text, std::nullopt};
    if (move.items[1].items[1].kind == Json::kString) m.model = move.items[1].items[1].text;
    out.push_back(std::move(m));
  }
  return out;
}

std::optional<DecomposedReferenceAlignment> alignment_of(const Json& value) {
  if (value.kind == Json::kNull) return std::nullopt;
  return DecomposedReferenceAlignment{moves_of(value.items[0]), static_cast<long long>(value.items[1].number)};
}

std::string show(const std::vector<std::string>& trace) {
  std::string out;
  for (const auto& e : trace) out += e + " ";
  return out;
}

std::string show(const DecomposedReferenceResult& r) {
  std::ostringstream out;
  out << "cost " << r.cost << " [";
  for (const auto& [log, model] : r.alignment) out << "(" << log << "," << (model ? *model : "None") << ")";
  out << "]";
  if (r.fitness) out << " fitness " << *r.fitness << " bwc " << *r.bwc;
  return out.str();
}

std::optional<DecomposedReferenceResult> result_of(const Json& value) {
  if (value.kind == Json::kNull) return std::nullopt;
  DecomposedReferenceResult out;
  out.cost = static_cast<long long>(value.get("cost")->number);
  for (const auto& pair : value.get("alignment")->items)
    out.alignment.emplace_back(pair.items[0].text, pair.items[1].kind == Json::kString ? std::optional(pair.items[1].text) : std::nullopt);
  if (const auto* fitness = value.get("fitness")) out.fitness = fitness->number;
  if (const auto* bwc = value.get("bwc")) out.bwc = static_cast<long long>(bwc->number);
  return out;
}

bool same(const DecomposedReferenceResult& a, const DecomposedReferenceResult& b) {
  if (a.cost != b.cost || a.alignment != b.alignment || a.bwc != b.bwc || a.fitness.has_value() != b.fitness.has_value()) return false;
  return !a.fitness || std::abs(*a.fitness - *b.fitness) < 1e-12;
}

struct Tally {
  std::size_t nets = 0, components = 0, merges = 0, aligns = 0, identical_aligns = 0, variants = 0, traces = 0,
              hangs = 0, native_traces = 0, native_same_cost = 0, native_hangs = 0;
};

void decompose_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto net = net_of(item.net);
  const auto mine = reference_decompose(net);
  std::vector<DecomposedReferenceComponent> expected;
  for (const auto& c : item.components.items) expected.push_back(component_of(c));
  ++tally.nets;
  tally.components += expected.size();
  if (mine != expected) failures.push_back(item.name + ": decomposition differs");
  const auto empty = alignment_of(item.bwc);
  const auto native = reference_decomposed_alignment(net, {});
  std::size_t visible = 0;
  for (const auto& move : empty->moves) visible += move.model.has_value();
  if (!native || native->cost != empty->cost || reference_decomposed_best_worst_cost(net) != visible)
    failures.push_back(item.name + ": best-worst cost differs");
}

void alignment_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto net = net_of(item.net);
  std::vector<DecomposedReferenceComponent> objects;
  for (const auto& c : item.components.items) objects.push_back(component_of(c));
  for (const auto& variant : item.variants) {
    for (const auto& event : variant.items[1].items) {
      const auto& tag = event.items[0].text;
      if (tag == "M") {
        std::vector<DecomposedReferenceComponent> parts;
        for (const auto& k : event.items[1].items) parts.push_back(objects.at(static_cast<std::size_t>(k.number)));
        const auto merged = reference_decomposed_merge(parts);
        if (static_cast<std::size_t>(event.items[2].number) != objects.size() || merged != component_of(event.items[3]))
          failures.push_back(item.name + ": merge differs");
        objects.push_back(merged);
        ++tally.merges;
      } else if (tag == "A") {
        const auto& component = objects.at(static_cast<std::size_t>(event.items[1].number));
        const auto projection = strings(event.items[2]);
        const auto expected = alignment_of(event.items[3]);
        const auto native = reference_decomposed_alignment(component.net, projection);
        ++tally.aligns;
        if (native.has_value() != expected.has_value() || (native && native->cost != expected->cost)) {
          failures.push_back(item.name + ": alignment cost differs on " + show(projection));
          continue;
        }
        if (native && native->moves == expected->moves) ++tally.identical_aligns;
        if (expected) {
          long long cost = 0;
          for (const auto& move : expected->moves) cost += reference_decomposed_move_cost(move);
          if (cost != expected->cost) failures.push_back(item.name + ": move costs do not add up");
        }
      }
    }
  }
}

void replay_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto net = net_of(item.net);
  std::vector<std::vector<std::string>> traces;
  for (const auto& t : item.head.items[1].items) traces.push_back(strings(t));
  std::size_t variant = static_cast<std::size_t>(-1), event = 0;
  std::string problem;
  auto next = [&](const char* tag) -> const Json* {
    if (variant >= item.variants.size()) return nullptr;
    const auto& events = item.variants[variant].items[1].items;
    if (event >= events.size() || events[event].items[0].text != tag) return nullptr;
    return &events[event++];
  };
  auto fail = [&](const std::string& what) {
    if (problem.empty()) problem = what;
  };
  DecomposedReferenceOracle oracle;
  oracle.variant = [&](const std::vector<std::string>& trace) {
    if (variant != static_cast<std::size_t>(-1) && event != item.variants[variant].items[1].items.size()) fail("unmatched events");
    ++variant;
    event = 0;
    if (variant >= item.variants.size() || strings(item.variants[variant].items[0]) != trace) fail("variant order differs at " + show(trace));
    ++tally.variants;
  };
  oracle.align = [&](std::size_t component, const DecomposedReferenceComponent&, const std::vector<std::string>& projection)
      -> std::optional<DecomposedReferenceAlignment> {
    const auto* e = next("A");
    if (!e || static_cast<std::size_t>(e->items[1].number) != component || strings(e->items[2]) != projection) {
      fail("align call differs on component " + std::to_string(component) + " " + show(projection));
      return std::nullopt;
    }
    return alignment_of(e->items[3]);
  };
  oracle.merged = [&](const std::vector<std::size_t>& inputs, std::size_t merged) {
    const auto* e = next("M");
    std::vector<std::size_t> expected;
    if (e) for (const auto& k : e->items[1].items) expected.push_back(static_cast<std::size_t>(k.number));
    if (!e || expected != inputs || static_cast<std::size_t>(e->items[2].number) != merged) fail("merge differs");
  };
  oracle.recomposed = [&](const std::vector<std::size_t>& components) {
    const auto* e = next("R");
    std::vector<std::size_t> expected;
    if (e) for (const auto& k : e->items[1].items) expected.push_back(static_cast<std::size_t>(k.number));
    if (!e || expected != components) fail("recompose components differ");
  };
  const bool hangs = item.apply.kind == Json::kString;
  try {
    const auto results = reference_decomposed_alignments(net, traces, oracle);
    if (hangs) {
      failures.push_back(item.name + ": the reference never returns but native does");
    } else {
      for (std::size_t k = 0; k < traces.size(); ++k) {
        ++tally.traces;
        const auto expected = result_of(item.apply.items[k]);
        if (!expected || !same(*expected, results[k]))
          fail("trace " + show(traces[k]) + ": " + show(results[k]) + " vs " + (expected ? show(*expected) : "None"));
      }
      if (variant + 1 != item.variants.size()) fail("variant count differs");
    }
  } catch (const DecomposedReferenceError& error) {
    if (!hangs) fail(std::string("raises: ") + error.what());
    else ++tally.hangs;
  }
  if (!problem.empty()) failures.push_back(item.name + ": " + problem);
}

void native_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto net = net_of(item.net);
  std::vector<std::vector<std::string>> traces;
  for (const auto& t : item.head.items[1].items) traces.push_back(strings(t));
  try {
    const auto results = reference_decomposed_alignments(net, traces);
    for (std::size_t k = 0; k < traces.size(); ++k) {
      ++tally.native_traces;
      long long cost = 0;
      for (const auto& [log, model] : results[k].alignment)
        cost += reference_decomposed_move_cost({log == ">>" ? ">>" : "t", model == ">>" ? ">>" : "m", log, model});
      if (cost != results[k].cost) failures.push_back(item.name + ": native cost does not follow from the alignment");
      if (item.apply.kind != Json::kString) {
        const auto expected = result_of(item.apply.items[k]);
        if (expected && expected->cost == results[k].cost) ++tally.native_same_cost;
      }
    }
  } catch (const DecomposedReferenceError&) {
    ++tally.native_hangs;
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: minera_decomposed_parity_tests <bank>\n";
    return 2;
  }
  std::vector<std::string> failures;
  Tally tally;
  for (const auto& item : read_bank(argv[1])) {
    try {
      decompose_case(item, tally, failures);
      alignment_case(item, tally, failures);
      replay_case(item, tally, failures);
      native_case(item, tally, failures);
    } catch (const std::exception& error) {
      failures.push_back(item.name + ": " + error.what());
    }
  }
  for (std::size_t k = 0; k < failures.size() && k < 30; ++k) std::cerr << failures[k] << "\n";
  std::cout << "decomposed recompos_maximal: " << tally.nets << " nets, " << tally.components << " components, "
            << tally.merges << " merges, " << tally.aligns << " component alignments (" << tally.identical_aligns
            << " identical), " << tally.variants << " variants replayed, " << tally.traces << " traces, " << tally.hangs
            << " logs where the reference never returns; native " << tally.native_traces << " traces ("
            << tally.native_same_cost << " same cost), " << tally.native_hangs << " logs raising; " << failures.size()
            << " failing cases\n";
  return failures.empty() ? 0 : 1;
}
