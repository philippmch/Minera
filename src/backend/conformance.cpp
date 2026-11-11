#include "minera/conformance.hpp"
#include "minera/cancellation.hpp"
#include "minera/token_replay_error.hpp"
#include "evaluation_internal.hpp"
#include "petri_replay_internal.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"
#include "token_replay_context.hpp"
#include "reference_activity_selector.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {
namespace {

char32_t replay_key_codepoint(std::string_view text, std::size_t& position) {
  const auto first = static_cast<unsigned char>(text[position++]);
  char32_t ch = first;
  int continuation = 0;
  if (first >= 0xc2 && first <= 0xdf) {
    ch = first & 0x1f; continuation = 1;
  } else if (first >= 0xe0 && first <= 0xef) {
    ch = first & 0x0f; continuation = 2;
  } else if (first >= 0xf0 && first <= 0xf4) {
    ch = first & 0x07; continuation = 3;
  } else if (first >= 0x80) {
    throw std::invalid_argument("token replay: invalid UTF-8 selector");
  }
  for (int i = 0; i < continuation; ++i) {
    if (position == text.size())
      throw std::invalid_argument("token replay: invalid UTF-8 selector");
    const auto next = static_cast<unsigned char>(text[position++]);
    if ((next & 0xc0) != 0x80)
      throw std::invalid_argument("token replay: invalid UTF-8 selector");
    ch = (ch << 6) | (next & 0x3f);
  }
  if ((continuation == 1 && ch < 0x80) ||
      (continuation == 2 && ch < 0x800) ||
      (continuation == 3 && ch < 0x10000) || ch > 0x10ffff)
    throw std::invalid_argument("token replay: invalid UTF-8 selector");
  return ch;
}

std::string replay_key_repr(const std::string& text) {
  const char quote = text.find('\'') != std::string::npos &&
                             text.find('"') == std::string::npos
                         ? '"' : '\'';
  std::string result(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t position = 0; position < text.size();) {
    const auto start = position;
    const auto ch = replay_key_codepoint(text, position);
    if (ch == quote || ch == '\\') {
      result += '\\';
      result += static_cast<char>(ch);
    } else if (ch == '\n') result += "\\n";
    else if (ch == '\r') result += "\\r";
    else if (ch == '\t') result += "\\t";
    else if (!root_cause_unicode_printable(ch)) {
      const int digits = ch <= 0xff ? 2 : ch <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        result += hex[(ch >> shift) & 15];
    } else result.append(text, start, position - start);
  }
  result += quote;
  return result;
}

using Marking = std::vector<int>;

// Bounds the silent-transition search so a pathological model can never make a
// single replay step loop forever; reachable silent routing is shallow in the
// block-structured nets the inductive miner produces.
constexpr std::size_t kSilentSearchCap = 4096;

// Cap on the aligner's cross-variant bound cache (entries); past it, solves
// simply stop being remembered rather than growing memory without limit.
constexpr std::size_t kAlignerBoundCacheCap = 2'000'000;

// FNV-1a hash of a token vector, used to key every marking-indexed search
// (token replay's silent routing, the precision pass, and alignment) directly
// on the marking instead of formatting a string per state — these searches
// visit far too many states for that to be cheap.
struct MarkingHash {
  std::size_t operator()(const std::vector<int>& v) const noexcept {
    std::size_t h = 1469598103934665603ull;
    for (const int x : v) {
      h = (h ^ static_cast<std::size_t>(static_cast<unsigned>(x))) *
          1099511628211ull;
    }
    return h;
  }
};

// A view of a marking (a state's places without its trailing position), and
// MarkingHash/equality over markings that also accept such a view, so a map
// keyed by markings can be searched without building one.
struct MarkingPrefix {
  const int* data;
  std::size_t size;
};
struct MarkingPrefixHash {
  using is_transparent = void;
  std::size_t operator()(const MarkingPrefix& v) const noexcept {
    std::size_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < v.size; ++i)
      h = (h ^ static_cast<std::size_t>(static_cast<unsigned>(v.data[i]))) * 1099511628211ull;
    return h;
  }
  std::size_t operator()(const std::vector<int>& v) const noexcept {
    return (*this)(MarkingPrefix{v.data(), v.size()});
  }
};
struct MarkingPrefixEqual {
  using is_transparent = void;
  static bool same(const int* a, std::size_t na, const int* b, std::size_t nb) noexcept {
    return na == nb && std::equal(a, a + na, b);
  }
  bool operator()(const std::vector<int>& a, const std::vector<int>& b) const noexcept {
    return a == b;
  }
  bool operator()(const MarkingPrefix& a, const std::vector<int>& b) const noexcept {
    return same(a.data, a.size, b.data(), b.size());
  }
  bool operator()(const std::vector<int>& a, const MarkingPrefix& b) const noexcept {
    return same(a.data(), a.size(), b.data, b.size);
  }
};

// Independently owned open addressing for an identity set: equality uses IDs,
// while supplied public hashes determine collisions, growth and iteration.
std::vector<std::size_t> replay_identity_set_order(
    const std::vector<std::size_t>& values,
    const std::vector<std::int64_t>& hashes) {
  struct Entry { std::size_t id; std::uint64_t hash; };
  const auto insert = [](Entry entry, auto& table) {
    const auto mask = table.size() - 1;
    std::size_t index = static_cast<std::size_t>(entry.hash & mask);
    auto perturb = entry.hash;
    for (;;) {
      const std::size_t probes = index + 9 < table.size() ? 9 : 0;
      for (std::size_t offset = 0; offset <= probes; ++offset) {
        auto& slot = table[index + offset];
        if (!slot) { slot = entry; return true; }
        if (slot->id == entry.id) return false;
      }
      perturb >>= 5;
      index = (index * 5 + 1 + static_cast<std::size_t>(perturb)) & mask;
    }
  };
  std::vector<std::optional<Entry>> table(8);
  std::size_t used = 0;
  for (const auto id : values) {
    if (!insert({id, static_cast<std::uint64_t>(hashes.at(id))}, table)) continue;
    ++used;
    if (used * 5 >= (table.size() - 1) * 3) {
      std::size_t size = 8;
      const auto factor = used > 50000 ? 2 : 4;
      while (size <= used * factor) size *= 2;
      auto old = std::move(table);
      table = std::vector<std::optional<Entry>>(size);
      for (const auto& slot : old)
        if (slot) insert(*slot, table);
    }
  }
  std::vector<std::size_t> ordered;
  ordered.reserve(used);
  for (const auto& slot : table)
    if (slot) ordered.push_back(slot->id);
  return ordered;
}

Marking resolve_workflow_marking(const PetriNet& net,
                                 const Marking& requested,
                                 bool initial) {
  if (requested.empty()) {
    Marking marking(net.place_count(), 0);
    const std::size_t place = initial ? net.source() : net.sink();
    if (place < marking.size()) marking[place] = 1;
    return marking;
  }
  if (requested.size() != net.place_count()) {
    throw std::invalid_argument(initial
                                    ? "initial marking dimension does not match net"
                                    : "final marking dimension does not match net");
  }
  if (std::any_of(requested.begin(), requested.end(),
                  [](int tokens) { return tokens < 0; })) {
    throw std::invalid_argument(initial ? "initial marking contains a negative token count"
                                        : "final marking contains a negative token count");
  }
  return requested;
}

template <class MarkingType>
bool marking_covers(const MarkingType& marking, const MarkingType& target) {
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (marking[place] < target[place]) return false;
  }
  return true;
}

template <class MarkingType>
bool marking_has_final_places(const MarkingType& marking, const MarkingType& target) {
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (target[place] > 0 && marking[place] <= 0) return false;
  }
  return true;
}

bool valid_explicit_marking(const PetriNet& net, const Marking& marking) {
  return marking.empty() ||
         (marking.size() == net.place_count() &&
          std::none_of(marking.begin(), marking.end(),
                       [](int tokens) { return tokens < 0; }));
}

bool valid_token_replay_variant(TokenReplayVariant variant) {
  switch (variant) {
    case TokenReplayVariant::kForward:
    case TokenReplayVariant::kBackwards:
      return true;
  }
  return false;
}

void validate_token_replay_event_limit(const EventLog& log,
                                     const TokenReplayOptions& options) {
  if (!options.max_rows_to_materialize) return;
  std::size_t available = *options.max_rows_to_materialize;
  for (const auto& trace : log.traces) {
    if (trace.events.size() > available)
      throw std::length_error("token replay input exceeds max_rows_to_materialize");
    available -= trace.events.size();
  }
}

bool valid_alignment_variant(AlignmentVariant variant) {
  switch (variant) {
    case AlignmentVariant::kStateEquationAStar:
    case AlignmentVariant::kDijkstraNoHeuristics:
    case AlignmentVariant::kDijkstraLessMemory:
    case AlignmentVariant::kDiscountedAStar:
    case AlignmentVariant::kGeneratorDijkstraNoHeuristics:
    case AlignmentVariant::kGeneratorDijkstraLessMemory:
      return true;
  }
  return false;
}

bool valid_alignment_cost_profile(AlignmentCostProfile profile) {
  switch (profile) {
    case AlignmentCostProfile::kLegacyUnit:
    case AlignmentCostProfile::kStandard:
      return true;
  }
  return false;
}

struct ResolvedReplayMarkings {
  TokenMarking initial, final;
  detail::ReplayMarkingKeys initial_keys, final_keys;
  std::vector<std::size_t> initial_order;
  std::optional<TokenReplayMarkingDomain> domain;
};

ResolvedReplayMarkings resolve_replay_markings(
    const PetriNet& net, const std::vector<int>& initial,
    const std::vector<int>& final, const TokenReplayOptions* options) {
  const auto* exact_initial = options && options->exact_initial_marking
      ? &*options->exact_initial_marking : nullptr;
  const auto* exact_final = options && options->exact_final_marking
      ? &*options->exact_final_marking : nullptr;
  const bool default_initial = exact_initial ? exact_initial->empty() : initial.empty();
  const bool default_final = exact_final ? exact_final->empty() : final.empty();
  const auto& discovery = net.discovery_markings();
  std::set<std::size_t> used_detached;
  if (discovery) {
    const auto collect = [&](const auto& entries) {
      for (const auto& entry : entries)
        if (entry.place.kind == PetriMarkingPlaceKind::kDetached)
          used_detached.insert(entry.place.id);
    };
    if (default_initial) collect(discovery->initial);
    if (default_final) collect(discovery->final);
  }
  ResolvedReplayMarkings result;
  std::unordered_map<std::size_t, std::size_t> detached_slots;
  if (!used_detached.empty()) {
    for (const auto& transition : net.transitions()) {
      const auto validate = [&](const auto& places) {
        for (const auto place : places)
          if (place >= net.place_count())
            throw std::invalid_argument("token replay arc place is outside root topology");
      };
      validate(transition.inputs); validate(transition.outputs);
      validate(transition.reset_inputs); validate(transition.inhibitor_inputs);
    }
    auto& domain = result.domain.emplace();
    for (std::size_t p = 0; p < net.place_count(); ++p)
      domain.places.push_back({PetriMarkingPlaceKind::kRoot, p});
    for (const auto& place : discovery->detached_places) {
      if (!used_detached.contains(place.id)) continue;
      detached_slots.emplace(place.id, domain.places.size());
      domain.places.push_back({PetriMarkingPlaceKind::kDetached, place.id});
      domain.detached_places.push_back(place);
    }
  }
  const auto size = result.domain ? result.domain->places.size() : net.place_count();
  const auto resolve = [&](bool first, const auto& legacy, const TokenMarking* exact,
                           bool use_default, auto& marking, auto& keys) {
    marking.resize(size, 0);
    keys.resize(size, false);
    if (use_default && discovery) {
      for (const auto& entry : first ? discovery->initial : discovery->final) {
        const auto slot = entry.place.kind == PetriMarkingPlaceKind::kRoot
            ? entry.place.id : detached_slots.at(entry.place.id);
        marking[slot] = entry.tokens;
        keys[slot] = true;
        if (first) result.initial_order.push_back(slot);
      }
      return;
    }
    TokenMarking values;
    if (exact && !use_default) {
      if (exact->size() != net.place_count())
        throw std::invalid_argument("exact marking dimension does not match net");
      if (options->variant == TokenReplayVariant::kBackwards &&
          std::any_of(exact->begin(), exact->end(), [](const auto& n) { return n < 0; }))
        throw std::invalid_argument("exact input marking contains a negative token count");
      values = *exact;
    } else {
      values = TokenMarking(resolve_workflow_marking(net, exact ? std::vector<int>{} : legacy, first));
    }
    std::copy(values.begin(), values.end(), marking.begin());
    for (std::size_t p = 0; p < values.size(); ++p) {
      keys[p] = values[p] != 0;
      if (first && keys[p]) result.initial_order.push_back(p);
    }
  };
  resolve(true, initial, exact_initial, default_initial, result.initial, result.initial_keys);
  resolve(false, final, exact_final, default_final, result.final, result.final_keys);
  return result;
}

detail::ReplayMarkingKeys resolve_domain_marking_keys(
    const detail::ReplayMarkingKeys& configured,
    const detail::ReplayMarkingKeys& defaults, const TokenMarking& marking,
    std::size_t roots, std::string_view name) {
  if (configured.empty()) return defaults;
  auto keys = configured;
  if (keys.size() == roots && roots < marking.size()) {
    keys.resize(marking.size(), false);
    for (std::size_t p = roots; p < keys.size(); ++p) keys[p] = defaults[p];
  }
  return detail::resolve_replay_marking_keys(keys, marking, name);
}

// Holds the net plus the lookups token replay needs and drives both the
// fitness and precision passes over a single shared translation.
// Token replay reads every place-to-transition arc as an ordinary input of its
// weight, so reset and inhibitor arcs consume like normal arcs there.
std::optional<PetriNet> ordinary_token_replay_net(const PetriNet& net) {
  if (!net.has_extended_arcs()) return std::nullopt;
  PetriNetLiteralStorage storage;
  storage.place_count = net.place_count();
  storage.source_storage = net.source();
  storage.sink_storage = net.sink();
  storage.has_source = net.has_source();
  storage.has_sink = net.has_sink();
  storage.transitions = net.transitions();
  storage.activities = net.activities();
  storage.discovery_markings = net.discovery_markings();
  storage.transition_names = net.transition_names();
  storage.place_names = net.place_names();
  for (auto& transition : storage.transitions) {
    if (!transition.has_extended_arcs()) continue;
    std::vector<PetriArcWeight> weights;
    if (transition.input_weights.empty()) {
      // Repeated-unit endpoints are one arc whose weight is their multiplicity.
      std::vector<std::size_t> places;
      for (const auto place : transition.inputs) {
        const auto found = std::find(places.begin(), places.end(), place);
        if (found != places.end()) {
          weights[found - places.begin()].magnitude += 1;
          continue;
        }
        places.push_back(place);
        weights.push_back({});
      }
      transition.inputs = std::move(places);
    } else {
      weights = transition.input_weights;
    }
    for (std::size_t index = 0; index < transition.reset_inputs.size(); ++index) {
      transition.inputs.push_back(transition.reset_inputs[index]);
      weights.push_back(transition.reset_arc_weight(index));
    }
    for (std::size_t index = 0; index < transition.inhibitor_inputs.size(); ++index) {
      transition.inputs.push_back(transition.inhibitor_inputs[index]);
      weights.push_back(transition.inhibitor_arc_weight(index));
    }
    if (transition.output_weights.empty())
      transition.output_weights.assign(transition.outputs.size(), PetriArcWeight{});
    transition.input_weights = std::move(weights);
    transition.reset_inputs.clear(); transition.reset_weights.clear();
    transition.inhibitor_inputs.clear(); transition.inhibitor_weights.clear();
  }
  return PetriNet::from_literal_storage(std::move(storage));
}

class Replayer {
 public:
  using Marking = TokenMarking;
  using MarkingHash = TokenMarkingHash;
 private:
  // Arc weights as token counts, one per input and output arc.
  struct ArcWeights {
    std::vector<TokenInteger> inputs;
    std::vector<TokenInteger> outputs;
  };
  struct InputCheck {
    std::size_t place = 0;
    TokenInteger required = 0;
  };
  struct InputChecks {
    std::size_t places = 0;  // marking size the checks were built for
    bool never = false;      // an input arc lies outside such markings
    bool per_call = false;   // an invalid arc weight: check arc by arc as before
    std::vector<InputCheck> checks;
  };
  struct ForwardConfiguration {
    detail::ReplayMarkingKeys initial_keys;
    detail::ReplayMarkingKeys final_keys;
    std::vector<std::size_t> initial_order;
    std::vector<std::string> place_names;
    std::vector<std::size_t> place_name_rank;  // by slot: rank among the distinct names
    PetriTransitionNameList transition_names;
    detail::CleaningComponents components;
  };
 public:
  explicit Replayer(const PetriNet& net, const std::vector<int>& initial = {},
                    const std::vector<int>& final = {},
                    const TokenReplayOptions* replay_options = nullptr,
                    bool match_null_activity = false)
      : ordinary_net_(replay_options ? ordinary_token_replay_net(net) : std::nullopt),
        net_(ordinary_net_ ? *ordinary_net_ : net),
        markings_(resolve_replay_markings(net, initial, final, replay_options)),
        initial_(std::move(markings_.initial)), final_(std::move(markings_.final)),
        full_result_(match_null_activity) {
    if (replay_options && replay_options->transition_identity_hashes &&
        replay_options->transition_identity_hashes->size() != net_.transitions().size())
      throw std::invalid_argument("transition_identity_hashes must contain one entry per transition");
    if (replay_options && replay_options->place_outgoing_transition_order) {
      const auto& outgoing = *replay_options->place_outgoing_transition_order;
      if (outgoing.size() != initial_.size())
        throw std::invalid_argument("place_outgoing_transition_order must contain one row per marking slot");
      for (std::size_t place = 0; place < outgoing.size(); ++place) {
        const auto adjacent = [&](std::size_t transition) {
          const auto& t = net_.transitions()[transition];
          return std::find(t.inputs.begin(), t.inputs.end(), place) != t.inputs.end() ||
                 std::find(t.reset_inputs.begin(), t.reset_inputs.end(), place) != t.reset_inputs.end() ||
                 std::find(t.inhibitor_inputs.begin(), t.inhibitor_inputs.end(), place) != t.inhibitor_inputs.end();
        };
        std::vector<bool> listed(net_.transitions().size(), false);
        for (const auto transition : outgoing[place]) {
          if (transition >= listed.size())
            throw std::invalid_argument("place_outgoing_transition_order contains an invalid transition index");
          if (!adjacent(transition))
            throw std::invalid_argument("place_outgoing_transition_order contains a non-adjacent transition");
          listed[transition] = true;
        }
        for (std::size_t transition = 0; transition < listed.size(); ++transition)
          if (adjacent(transition) && !listed[transition])
            throw std::invalid_argument("place_outgoing_transition_order must list every outgoing transition");
      }
    }
    if (replay_options && replay_options->variant == TokenReplayVariant::kBackwards) {
      (void)detail::replay_transition_names(replay_options->nullable_transition_names,
                                          replay_options->transition_names, net_);
      markings_.initial_keys = resolve_domain_marking_keys(replay_options->initial_marking_keys,
          markings_.initial_keys, initial_, net_.place_count(), "initial_marking_keys");
      markings_.final_keys = resolve_domain_marking_keys(replay_options->final_marking_keys,
          markings_.final_keys, final_, net_.place_count(), "final_marking_keys");
    }
    for (std::size_t i = 0; i < net_.activities().size(); ++i) {
      name_to_activity_.emplace(net_.activities()[i], i);
    }
    transitions_by_activity_.resize(net_.activities().size() + (match_null_activity ? 1 : 0));
    for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
      const auto& transition = net_.transitions()[t];
      if (transition.silent()) {
        silent_transitions_.push_back(t);
        if (match_null_activity) transitions_by_activity_.back().push_back(t);
      } else {
        transitions_by_activity_[*transition.activity].push_back(t);
      }
    }
    extended_arcs_ = net_.has_extended_arcs();
    arc_weights_.resize(net_.transitions().size());
    input_checks_.resize(net_.transitions().size());
    fire_inputs_.resize(net_.transitions().size());
    build_place_closure();
    if (replay_options && replay_options->variant == TokenReplayVariant::kForward)
      forward_configuration_ = prepare_forward(*replay_options);
  }

  [[nodiscard]] std::optional<std::size_t> activity_id(
      const std::string& name) const {
    const auto it = name_to_activity_.find(name);
    return it == name_to_activity_.end() ? std::nullopt
                                         : std::optional<std::size_t>(it->second);
  }

  [[nodiscard]] std::optional<std::size_t> null_activity_id() const {
    return transitions_by_activity_.size() > net_.activities().size() &&
           !silent_transitions_.empty()
        ? std::optional<std::size_t>(net_.activities().size()) : std::nullopt;
  }

  // Token-replay bookkeeping for one trace.
  struct Counts {
    TokenInteger produced = 0;
    TokenInteger consumed = 0;
    TokenInteger missing = 0;
    TokenInteger remaining = 0;
    TokenInteger missing_before_final_check = 0;
    bool has_unknown_activity = false;
    bool forward_replay = false;
    std::vector<TokenReplayPlaceDiagnostics> places;

    [[nodiscard]] bool forward_fitting(const TokenReplayOptions& options) const {
      return missing_before_final_check == 0 &&
          (!options.consider_remaining_in_fitness || remaining == 0) &&
          (!options.consider_activities_not_in_model_in_fitness || !has_unknown_activity);
    }

    [[nodiscard]] double fitness() const {
      if (forward_replay && (consumed <= 0 || produced <= 0)) return 1.0;
      if (!forward_replay && (consumed == 0 || produced == 0)) {
        throw std::domain_error(
            "backwards token-replay fitness is undefined when consumed or "
            "produced tokens are zero");
      }
      const double consume_term = consumed == 0 ? 0.0 : double(missing) / double(consumed);
      const double produce_term = produced == 0 ? 0.0 : double(remaining) / double(produced);
      return 0.5 * (1.0 - consume_term) + 0.5 * (1.0 - produce_term);
    }
  };

  // Forward replay state of one trace between two events.
  struct ForwardState {
    Marking marking;
    detail::ReplayMarkingKeys keys;
    std::vector<std::size_t> order;
    Counts counts;
  };

  // The prefix walk of token precision: the state before the first event and
  // one event at a time, exactly as replay() advances (forward options only).
  [[nodiscard]] ForwardState start_prefix_walk() const {
    if (!forward_configuration_)
      throw std::logic_error("prefix walk requires forward replay options");
    return initial_forward_state(*forward_configuration_);
  }
  // False once the event needed a missing token and replay stopped (with
  // stop_immediately_unfit); the state then holds the marking replay reached.
  [[nodiscard]] bool advance_prefix_walk(ForwardState& state,
                                         const std::optional<std::size_t>& step,
                                         const TokenReplayOptions& options) const {
    return forward_event(state, step, options, *forward_configuration_, nullptr, nullptr);
  }

  // The trace fitness replay() would report for a trace ending at `state`
  // (forward options without final routing): the final marking is consumed
  // from the reached marking exactly as replay() does after its last event.
  [[nodiscard]] double prefix_walk_fitness(const ForwardState& state) const {
    const auto& final_keys = forward_configuration_->final_keys;
    Counts counts;
    counts.forward_replay = true;
    counts.produced = state.counts.produced;
    counts.consumed = state.counts.consumed;
    counts.missing = state.counts.missing;
    counts.remaining = state.counts.remaining;
    for (std::size_t place = 0; place < state.marking.size(); ++place) {
      auto tokens = state.marking[place];
      counts.consumed += TokenInteger(final_[place]);
      if (final_keys[place] && (state.keys[place] || TokenInteger(final_[place]) > 0)) {
        if (tokens >= final_[place]) {
          tokens -= final_[place];
        } else {
          counts.missing += TokenInteger(final_[place] - tokens);
          tokens = 0;
        }
      }
      if (tokens != 0) counts.remaining += TokenInteger(tokens);
    }
    return counts.fitness();
  }

 private:
  [[nodiscard]] ForwardState initial_forward_state(const ForwardConfiguration& configuration) const {
    ForwardState state;
    state.marking = initial_marking();
    state.keys = configuration.initial_keys;
    state.order = configuration.initial_order;
    state.counts.forward_replay = true;
    state.counts.places.resize(initial_.size());
    for (std::size_t place = 0; place < initial_.size(); ++place) {
      const auto tokens = TokenInteger(initial_[place]);
      state.counts.produced += tokens;
      state.counts.places[place].produced += tokens;
    }
    return state;
  }

  // One event of forward replay. Returns false when replay stops at this
  // event (stop_immediately_unfit and a missing token).
  [[nodiscard]] bool forward_event(ForwardState& state, const std::optional<std::size_t>& step,
                                   const TokenReplayOptions& options,
                                   const ForwardConfiguration& configuration,
                                   std::vector<std::uint64_t>* executions,
                                   TokenReplayTraceDiagnostics* diagnostics) const {
    auto& marking = state.marking;
    auto& keys = state.keys;
    auto& order = state.order;
    auto& counts = state.counts;
    const auto& place_names = configuration.place_names;
    const auto& transition_names = configuration.transition_names;
    const auto& components = configuration.components;

    const auto emit = [&](std::size_t transition, bool event) {
      if (!forward_fire(marking, keys, transition, counts, &order)) return;
      if (executions) ++(*executions)[transition];
      if (diagnostics) {
        diagnostics->activated_transitions.push_back(transition);
        if (event) diagnostics->event_diagnostics.back().activated_transitions.push_back(transition);
      }
    };
    if (diagnostics) {
      diagnostics->event_diagnostics.push_back({diagnostics->replayed_events, {}, {}});
      ++diagnostics->replayed_events;
    }
    if (!step || transitions_by_activity_[*step].empty()) {
      counts.has_unknown_activity = true;
      if (diagnostics)
        diagnostics->unknown_event_indices.push_back(diagnostics->replayed_events - 1);
      return true;
    }
    const auto& targets = transitions_by_activity_[*step];
    auto chosen = *std::max_element(targets.begin(), targets.end(), [&](auto left, auto right) {
      return std::tie(transition_names[left], left) < std::tie(transition_names[right], right);
    });
    if (options.transition_identity_hashes && !extended_arcs_) {
      // The public enabled set includes every activity and silent transition.
      // Filtering matching activities before constructing that set changes
      // its iteration order when collisions or table growth are involved.
      std::vector<std::size_t> enabled_candidates;
      for (std::size_t transition = 0; transition < net_.transitions().size(); ++transition)
        if (enabled(marking, transition)) enabled_candidates.push_back(transition);
      for (const auto transition : replay_identity_set_order(
               enabled_candidates, *options.transition_identity_hashes)) {
        if (std::find(targets.begin(), targets.end(), transition) != targets.end()) {
          chosen = transition;
          break;
        }
      }
    } else {
      for (const auto transition : targets) {
        if (enabled(marking, transition)) { chosen = transition; break; }
      }
    }
    if (!enabled(marking, chosen) && options.walk_through_hidden_transitions) {
      if (extended_arcs_ && !has_supplied_hidden_paths(options)) {
        const auto path = silent_path_to_enable(marking, std::vector<std::size_t>{chosen});
        if (path) for (const auto transition : *path) emit(transition, true);
      } else {
        forward_route_hidden(marking, keys, chosen, place_names, &configuration.place_name_rank,
                             options,
                             [&](auto transition) { emit(transition, true); });
      }
    }
    const bool forced = !enabled(marking, chosen);
    if (forced && diagnostics) {
      diagnostics->transitions_with_problems.push_back(chosen);
      diagnostics->event_diagnostics.back().transitions_with_problems.push_back(chosen);
    }
    if (forced && options.stop_immediately_unfit) {
      ++counts.missing;
      if (diagnostics) diagnostics->replay_stopped = true;
      return false;
    }
    // A null-selected silent event has no PLTR transition-map key. This
    // rejection occurs during the event, before per-trace float conversion.
    if (options.enable_pltr_fitness && *step == net_.activities().size())
      throw std::invalid_argument("null transition PLTR key");
    if (full_result_ && options.enable_pltr_fitness &&
        net_.transitions()[chosen].activity &&
        net_.activities()[*net_.transitions()[chosen].activity].empty())
      throw TokenReplayTransitionKeyError(chosen);
    const auto before_cleaning = forced && !components.empty()
        ? keys : detail::ReplayMarkingKeys{};
    emit(chosen, true);
    if (!before_cleaning.empty()) {
      const auto introduced_places = detail::cleaning_introduced_places(keys, before_cleaning, place_names);
      const auto unique_names = detail::cleaning_unique_place_names(keys, place_names);
      for (const auto& component : components) {
        const bool introduced = std::any_of(component.begin(), component.end(),
            [&](auto place) { return keys[place] && introduced_places[place]; });
        if (!introduced) continue;
        for (const auto place : component) {
          if (keys[place] && before_cleaning[place] && unique_names[place]) {
            marking[place] = 0;
            keys[place] = false;
            std::erase(order, place);
            ++counts.remaining;
            ++counts.places[place].remaining;
            if (diagnostics && std::find(diagnostics->cleaned_place_indices.begin(),
                diagnostics->cleaned_place_indices.end(), place) == diagnostics->cleaned_place_indices.end())
              diagnostics->cleaned_place_indices.push_back(place);
          }
        }
      }
    }
    return true;
  }

 public:
  // Replays a trace (activity ids, std::nullopt = activity absent from the
  // model) and returns the token counts. Perfect replay yields missing == 0
  // and remaining == 0. When `executions` is given (sized to the net's
  // transition count) each transition the replay fires — silent routing
  // included — is tallied into it, feeding generalization.
  [[nodiscard]] Counts replay(
      const std::vector<std::optional<std::size_t>>& trace,
      std::vector<std::uint64_t>* executions = nullptr,
      const TokenReplayOptions& options = {},
      TokenReplayTraceDiagnostics* diagnostics = nullptr) const {
    std::optional<ForwardConfiguration> local_configuration;
    if (!forward_configuration_) local_configuration = prepare_forward(options);
    const auto& configuration = forward_configuration_ ? *forward_configuration_ : *local_configuration;
    ForwardState state = initial_forward_state(configuration);
    auto& marking = state.marking;
    auto& keys = state.keys;
    auto& order = state.order;
    auto& counts = state.counts;
    const auto& final_keys = configuration.final_keys;
    const auto& place_names = configuration.place_names;
    if (diagnostics) diagnostics->marking_domain = markings_.domain;
    const auto emit = [&](std::size_t transition, bool event) {
      if (!forward_fire(marking, keys, transition, counts, &order)) return;
      if (executions) ++(*executions)[transition];
      if (diagnostics) {
        diagnostics->activated_transitions.push_back(transition);
        if (event) diagnostics->event_diagnostics.back().activated_transitions.push_back(transition);
      }
    };
    for (const auto& step : trace)
      if (!forward_event(state, step, options, configuration, executions, diagnostics)) break;
    const auto final_present = [&]() {
      for (std::size_t place = 0; place < keys.size(); ++place)
        if (final_keys[place] && !keys[place]) return false;
      return true;
    };
    if (options.try_to_reach_final_marking_through_hidden) {
      if (extended_arcs_ && !has_supplied_hidden_paths(options)) {
        // Inhibitor/reset-only transitions need not be reachable from a
        // marked ordinary input. Preserve enabled-state routing for these nets.
        const auto path = silent_path_to_final(marking);
        if (path) for (const auto transition : *path) emit(transition, false);
      } else for (std::size_t pass = 0; pass < 6 && !final_present(); ++pass) {
        const auto paths = forward_place_paths(keys, final_keys, place_names, options,
                                               &configuration.place_name_rank);
        bool fired = false;
        for (const auto& path : paths) {
          for (const auto transition : path.transitions) {
            if (!enabled(marking, transition)) break;
            emit(transition, false);
            fired = true;
          }
          if (final_present()) break;
        }
        if (!fired) break;
      }
    }
    counts.missing_before_final_check = counts.missing;
    if (diagnostics) {
      diagnostics->reached_marking = marking;
      diagnostics->reached_marking_keys = keys;
      if (net_.discovery_markings()) diagnostics->reached_marking_order = order;
    }
    for (std::size_t place = 0; place < marking.size(); ++place) {
      const auto required = TokenInteger(final_[place]);
      counts.consumed += required;
      counts.places[place].consumed += required;
      // A signed final entry only adjusts a place present in the reached
      // marking. Positive absent entries still contribute missing tokens.
      if (final_keys[place] && (keys[place] || required > 0)) {
        if (marking[place] >= final_[place]) {
          marking[place] -= final_[place];
        } else {
          const auto missing = TokenInteger(final_[place] - marking[place]);
          counts.missing += missing;
          counts.places[place].missing += missing;
          marking[place] = 0;
        }
      }
      if (marking[place] != 0) {
        const auto tokens = TokenInteger(marking[place]);
        counts.remaining += tokens;
        counts.places[place].remaining += tokens;
      }
    }
    return counts;
  }

  struct BackwardsOutcome {
    Counts counts;
    bool fitting = true;
    Marking marking;
  };

  // Legacy backwards token replay. Unlike forward replay, unknown
  // labels are ignored and silent routing is discovered by walking required
  // input markings backwards from a visible transition.
  [[nodiscard]] BackwardsOutcome replay_backwards(
      const std::vector<std::optional<std::size_t>>& trace,
      std::vector<std::uint64_t>* executions = nullptr,
      bool stop_immediately_unfit = false,
      bool /*walk_through_hidden_transitions*/ = true,
      TokenReplayTraceDiagnostics* diagnostics = nullptr) const {
    // The reference backwards variation always walks silent transitions; its
    // walk_through_hidden_trans parameter has no observable effect.
    constexpr bool walk_through_hidden_transitions = true;
    Marking marking = initial_marking();
    BackwardsOutcome outcome;
    auto& counts = outcome.counts;
    counts.places.resize(initial_.size());
    if (diagnostics) diagnostics->marking_domain = markings_.domain;
    for (std::size_t place = 0; place < initial_.size(); ++place) {
      const auto tokens = TokenInteger(initial_[place]);
      counts.produced += tokens;
      counts.places[place].produced += tokens;
    }

    for (const auto& step : trace) {
      if (diagnostics) {
        diagnostics->event_diagnostics.push_back({diagnostics->replayed_events, {}, {}});
        ++diagnostics->replayed_events;
      }
      // The deprecated variation silently discards labels absent from the net.
      if (!step || transitions_by_activity_[*step].empty()) {
        if (diagnostics)
          diagnostics->unknown_event_indices.push_back(diagnostics->replayed_events - 1);
        continue;
      }
      const auto& targets = transitions_by_activity_[*step];
      bool fired_any = false;
      for (const std::size_t t : targets) {
        // Check each duplicate-label transition against the evolving marking;
        // an earlier firing may enable a later transition.
        if (enabled(marking, t)) {
          fire(marking, t, counts);
          if (executions) ++(*executions)[t];
          if (diagnostics) {
            diagnostics->activated_transitions.push_back(t);
            diagnostics->event_diagnostics.back().activated_transitions.push_back(t);
          }
          fired_any = true;
        }
      }
      if (fired_any) {
        continue;
      }

      if (targets.size() != 1) {
        outcome.fitting = false;  // ambiguous duplicate label: interrupt trace
        if (diagnostics) diagnostics->replay_stopped = true;
        break;
      }

      const std::size_t visible = targets.front();
      const auto path = walk_through_hidden_transitions
                            ? reverse_silent_path(marking, visible)
                            : std::nullopt;
      if (path) {
        for (const std::size_t s : *path) {
          fire(marking, s, counts);
          if (executions) ++(*executions)[s];
          if (diagnostics) {
            diagnostics->activated_transitions.push_back(s);
            diagnostics->event_diagnostics.back().activated_transitions.push_back(s);
          }
        }
        fire(marking, visible, counts);
        if (executions) ++(*executions)[visible];
        if (diagnostics) {
          diagnostics->activated_transitions.push_back(visible);
          diagnostics->event_diagnostics.back().activated_transitions.push_back(visible);
        }
      } else {
        // No route: weak-fire the unique visible transition, creating any
        // missing inputs and retaining the deviation for fitness. Parallel
        // arcs can leave the per-place preset satisfied; that firing still
        // counts its missing tokens but is not a fitness problem.
        const bool problem = !preset_satisfied(marking, visible);
        fire(marking, visible, counts);
        if (executions) ++(*executions)[visible];
        if (diagnostics) {
          diagnostics->activated_transitions.push_back(visible);
          diagnostics->event_diagnostics.back().activated_transitions.push_back(visible);
          if (problem) {
            diagnostics->transitions_with_problems.push_back(visible);
            diagnostics->event_diagnostics.back().transitions_with_problems.push_back(visible);
          }
        }
        if (problem) outcome.fitting = false;
        if (stop_immediately_unfit) {
          if (diagnostics) diagnostics->replay_stopped = true;
          break;
        }
      }
    }

    // Compare the exact requested final marking, with no end routing in the
    // legacy variation. Fitness is decided by this final comparison and the
    // firings above, not by tokens a fitting firing reported missing.
    const auto missing_before_final = counts.missing;
    for (std::size_t p = 0; p < marking.size(); ++p) {
      const auto& target = final_[p];
      if (markings_.final_keys[p] && marking[p] != 0) {
        const auto available = TokenInteger(marking[p]);
        counts.consumed += available;
        counts.places[p].consumed += available;
      }
      if (marking[p] > target) {
        const auto value = TokenInteger(marking[p] - target);
        counts.remaining += value;
        counts.places[p].remaining += value;
      } else if (marking[p] < target) {
        const auto value = TokenInteger(target - marking[p]);
        counts.missing += value;
        counts.places[p].missing += value;
      }
    }
    if (counts.missing != missing_before_final || counts.remaining != 0) outcome.fitting = false;
    outcome.marking = marking;
    if (diagnostics) diagnostics->reached_marking = marking;
    return outcome;
  }

  // Like replay(), but only for traces that replay cleanly, and returning what
  // fired instead of token counts: one entry per consumed event holding the
  // transitions fired for it — silent routing first, the event's visible
  // transition last — plus one final entry for the routing to the sink.
  // nullopt as soon as the replay deviates (an activity the net lacks, a
  // missing token, or leftover tokens at the end), because a forced replay's
  // firing sequence would not be a real path through the model.
  // `capped`, when given, is set when a silent-routing search on the way
  // stopped at its bound, so a nullopt may be the bound rather than a deviation.
  [[nodiscard]] std::optional<std::vector<std::vector<std::size_t>>>
  clean_firing_sequence(
      const std::vector<std::optional<std::size_t>>& trace,
      bool* capped = nullptr) const {
    Marking marking = initial_marking();
    std::vector<std::vector<std::size_t>> steps;
    steps.reserve(trace.size() + 1);
    for (const auto& step : trace) {
      if (!step || transitions_by_activity_[*step].empty()) {
        return std::nullopt;
      }
      const Step& result = cached_step(marking, *step);
      if (capped && result.capped) *capped = true;
      if (result.missing != 0) return std::nullopt;
      marking = result.marking;
      steps.push_back(result.fired);
    }
    const Step& finish = cached_finish(marking);
    if (capped && finish.capped) *capped = true;
    if (finish.missing != 0) return std::nullopt;
    marking = finish.marking;
    if (!marking_covers(marking, final_)) return std::nullopt;
    for (std::size_t place = 0; place < marking.size(); ++place) {
      marking[place] -= final_[place];
    }
    for (const auto& tokens : marking) {
      if (tokens > 0) return std::nullopt;
    }
    steps.push_back(finish.fired);
    return steps;
  }

  [[nodiscard]] Marking initial_marking() const { return initial_; }
  [[nodiscard]] const std::optional<TokenReplayMarkingDomain>& marking_domain() const {
    return markings_.domain;
  }

  // Advances a precision prefix using its enabled-state routing cache. The
  // forward trace replay separately retains partial structural routes and
  // key-aware cleaning; precision's prefix contract is independently audited.
  [[nodiscard]] bool advance(
      Marking& marking, std::optional<std::size_t> activity,
      bool walk_through_hidden_transitions = true) const {
    // Activities absent from the model are ignored by evaluator replay.
    if (!activity || transitions_by_activity_[*activity].empty()) return true;
    const auto& step = cached_step(marking, *activity, walk_through_hidden_transitions);
    marking = step.marking;
    return step.missing == 0;
  }

  // Activities whose transitions can be enabled from `marking` (possibly after
  // firing silent transitions) — the model's allowed next observable steps.
  [[nodiscard]] std::unordered_set<std::size_t> enabled_activities(
      const Marking& marking) const {
    // When every silent transition moves a single token (one input, one
    // output) and every visible transition needs a single place, tokens
    // wander the silent structure independently and the closure is exact and
    // cheap: an activity is allowed next iff some occupied place silently
    // reaches one of its input places. This covers the heuristic miner's
    // one-token nets — whose interleaved multi-token closures are far too
    // large to enumerate (and used to be cut off by the search cap, slightly
    // understating what the model allows).
    if (place_closure_exact_) {
      std::unordered_set<std::size_t> acts;
      for (std::size_t p = 0; p < marking.size(); ++p) {
        if (marking[p] <= 0) continue;
        for (const std::size_t a : place_enables_[p]) acts.insert(a);
      }
      return acts;
    }

    // General nets: an activity is allowed next if a transition for it is
    // enabled at this marking or at any marking reachable by firing silent
    // transitions. The exact answer needs a breadth-first silent closure
    // (bounded so a pathological model cannot loop forever) — but on a net
    // whose silent structure interleaves several tokens, that closure used to
    // run to its cap on every distinct marking, which dominated whole
    // conformance checks. Two sound cuts keep it honest and fast: activities
    // the relaxed reachability disproves are dropped before the search, and
    // the search stops the moment every surviving candidate has been found.
    std::unordered_set<std::size_t> acts;
    const auto reachable = relaxed_silent_places(marking);
    std::vector<std::size_t> remaining;
    for (std::size_t a = 0; a < transitions_by_activity_.size(); ++a) {
      bool possible = false;
      for (const std::size_t t : transitions_by_activity_[a]) {
        if (relaxed_enables(reachable, t)) {
          possible = true;
          break;
        }
      }
      if (!possible) continue;  // provably not enableable from here
      if (any_enabled(marking, transitions_by_activity_[a])) {
        acts.insert(a);
      } else {
        remaining.push_back(a);
      }
    }
    if (remaining.empty()) return acts;

    const auto collect = [&](const Marking& m) {
      for (std::size_t i = 0; i < remaining.size();) {
        if (any_enabled(m, transitions_by_activity_[remaining[i]])) {
          acts.insert(remaining[i]);
          remaining[i] = remaining.back();
          remaining.pop_back();
        } else {
          ++i;
        }
      }
    };

    std::unordered_set<Marking, MarkingHash> seen{marking};
    std::queue<Marking> frontier;
    frontier.push(marking);
    std::size_t expanded = 0;
    const std::size_t budget = detail::enabled_search_budget();
    while (!frontier.empty() && !remaining.empty()) {
      // Candidates are still open and markings are still unexplored: a
      // partial set would understate escaping edges, so refuse to answer.
      if (expanded++ >= budget) throw detail::SilentSearchBudgetExhausted();
      const Marking current = std::move(frontier.front());
      frontier.pop();
      for (const std::size_t s : silent_transitions_) {
        if (!enabled(current, s)) continue;
        Marking next = current;
        fire_marking(next, s);
        if (!seen.insert(next).second) continue;
        collect(next);
        if (remaining.empty()) break;
        frontier.push(std::move(next));
      }
    }
    return acts;
  }

  // The forward enabled-transition search's visit order and its (name,
  // label) classes. They depend on the net and the options alone, so a caller
  // that searches many markings with the same options builds them once.
  struct ForwardEnabledPlan {
    std::vector<std::int64_t> hashes;
    std::vector<std::size_t> order;
    std::vector<std::size_t> pair_ids;  // by transition
    std::size_t pair_count = 0;
    // The flat search's per-net tables for markings of `flat_places` slots
    // (see flat_forward_enabled), indexed by `order` position.
    std::size_t flat_places = std::numeric_limits<std::size_t>::max();
    std::vector<std::vector<std::uint32_t>> flat_readers;  // by place: positions whose enabling reads it
    std::vector<std::vector<std::uint32_t>> flat_touched;  // by position: places its firing may change
    std::vector<std::uint32_t> flat_pair;                  // by position: pair id
    std::vector<char> flat_silent;                         // by position
  };

  [[nodiscard]] ForwardEnabledPlan forward_enabled_plan(const TokenReplayOptions& options) const {
    const auto names = detail::replay_transition_names(
        options.nullable_transition_names, options.transition_names, net_);
    detail::validate_forward_transition_name_order(names);
    ForwardEnabledPlan plan;
    auto& hashes = plan.hashes;
    hashes.resize(net_.transitions().size());
    if (options.transition_identity_hashes) hashes = *options.transition_identity_hashes;
    else for (std::size_t t = 0; t < hashes.size(); ++t) hashes[t] = static_cast<std::int64_t>(t);
    auto& order = plan.order;
    order.resize(hashes.size());
    for (std::size_t t = 0; t < order.size(); ++t) order[t] = t;
    std::stable_sort(order.begin(), order.end(), [&](const auto left, const auto right) {
      return std::tie(names[left], hashes[left]) < std::tie(names[right], hashes[right]);
    });

    using Pair = std::pair<std::optional<std::string>, std::optional<std::string>>;
    std::map<Pair, std::size_t> pairs;
    plan.pair_ids.resize(order.size());
    for (const auto t : order) {
      const auto& transition = net_.transitions()[t];
      const auto label = transition.activity
          ? std::optional<std::string>(net_.activities().at(*transition.activity)) : std::nullopt;
      const auto [it, added] = pairs.emplace(Pair{names[t], label}, pairs.size());
      (void)added;
      plan.pair_ids[t] = it->second;
    }
    plan.pair_count = pairs.size();
    return plan;
  }

  // `budget`, when given, bounds the forward worklist's silent expansions and
  // reports exhaustion as SilentSearchBudgetExhausted (the precision root).
  // `plan_cache`, when given, keeps the forward plan across calls that pass
  // the same options.
  [[nodiscard]] std::vector<std::size_t> enabled_transition_ids(
      const Marking& marking,
      const TokenReplayOptions& options,
      std::optional<std::size_t> budget = std::nullopt,
      std::optional<ForwardEnabledPlan>* plan_cache = nullptr) const {
    if (extended_arcs_) {
      std::vector<std::size_t> result;
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!net_.transitions()[t].silent() &&
            silent_path_to_enable(marking, std::vector<std::size_t>{t}))
          result.push_back(t);
      }
      return result;
    }

    if (options.variant == TokenReplayVariant::kForward) {
      std::optional<ForwardEnabledPlan> local_plan;
      auto& plan_slot = plan_cache ? *plan_cache : local_plan;
      if (!plan_slot) plan_slot = forward_enabled_plan(options);
      const auto& plan = *plan_slot;
      const auto& order = plan.order;
      const auto& pair_ids = plan.pair_ids;
      const auto transition_count = order.size();
      const auto expansion_slots = kSilentSearchCap + 1;
      const std::size_t worklist_cap = transition_count != 0 &&
              expansion_slots > std::numeric_limits<std::size_t>::max() / transition_count
          ? std::numeric_limits<std::size_t>::max() : expansion_slots * transition_count;
      std::vector<std::size_t> selected;
      if (!flat_forward_enabled(marking, *plan_slot, budget, worklist_cap, selected)) {
        // The reference's FIFO worklist over (transition, latest marking)
        // entries. Each reached marking is interned once, so a worklist entry
        // and a visited (name, label, marking) triple hold its id instead of a
        // copy; ids are equal exactly when markings are, so the visit order,
        // the expansions counted against the caps and the result are unchanged.
        selected.clear();
        std::unordered_map<Marking, std::uint32_t, MarkingHash> interned;
        std::vector<const Marking*> markings;
        const auto intern = [&](Marking&& reached) {
          const auto [it, added] = interned.try_emplace(std::move(reached),
                                                        static_cast<std::uint32_t>(markings.size()));
          if (added) markings.push_back(&it->first);
          return it->second;
        };
        std::unordered_set<std::uint64_t> visited;  // pair id << 32 | marking id
        std::vector<std::uint32_t> latest(order.size(), 0);
        std::queue<std::size_t> pending;
        const auto discover = [&](std::uint32_t id) {
          const Marking& reached = *markings[id];
          for (const auto t : order) {
            if (!enabled(reached, t)) continue;
            if (pending.size() >= worklist_cap)
              throw std::runtime_error("enabled transition diagnostic worklist exceeded limit");
            latest[t] = id;
            pending.push(t);
          }
        };
        discover(intern(Marking(marking)));
        std::size_t expanded = 0;
        while (!pending.empty()) {
          const auto t = pending.front(); pending.pop();
          const auto id = latest[t];
          if (!visited.insert((std::uint64_t(pair_ids[t]) << 32) | id).second) continue;
          if (!net_.transitions()[t].silent()) { selected.push_back(t); continue; }
          if (budget && expanded >= *budget) throw detail::SilentSearchBudgetExhausted();
          if (expanded++ >= kSilentSearchCap && !budget)
            throw std::runtime_error("enabled transition diagnostic search exceeded limit");
          Marking next = *markings[id];
          fire_marking(next, t);
          discover(intern(std::move(next)));
        }
      }
      if (!options.transition_identity_hashes) {
        // Native IDs have an established presentation order. A foreign hash
        // order is meaningful only when its actual public hashes are supplied.
        std::sort(selected.begin(), selected.end());
        selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
        return selected;
      }
      return replay_identity_set_order(selected, plan.hashes);
    }

    const auto transition_names = detail::replay_transition_names(
        options.nullable_transition_names, options.transition_names, net_);
    std::vector<std::size_t> order(net_.transitions().size());
    for (std::size_t t = 0; t < order.size(); ++t) order[t] = t;
    if (options.variant == TokenReplayVariant::kForward) {
      detail::validate_forward_transition_name_order(transition_names);
      std::stable_sort(order.begin(), order.end(), [&](const auto left, const auto right) {
        return std::tie(transition_names[left], left) < std::tie(transition_names[right], right);
      });
    }

    std::vector<bool> visible_enabled(net_.transitions().size(), false);
    std::vector<std::optional<Marking>> latest_marking(net_.transitions().size());
    std::queue<std::size_t> pending;
    std::vector<std::unordered_set<Marking, MarkingHash>> visited(
        net_.transitions().size());
    const auto transition_count = net_.transitions().size();
    const auto expansion_slots = kSilentSearchCap + 1;
    const std::size_t worklist_cap =
        transition_count != 0 &&
                expansion_slots > std::numeric_limits<std::size_t>::max() /
                    transition_count
            ? std::numeric_limits<std::size_t>::max()
            : expansion_slots * transition_count;
    const auto enqueue = [&](std::size_t transition) {
      if (pending.size() >= worklist_cap)
        throw std::runtime_error("enabled transition diagnostic worklist exceeded limit");
      pending.push(transition);
    };
    const auto discover = [&](const Marking& reached) {
      // The public helper walks a FIFO worklist.  Every newly reached marking
      // refreshes the latest state for each enabled silent transition and
      // appends that transition, including duplicates, to the worklist.
      for (const auto transition : order) {
        if (!enabled(reached, transition)) continue;
        if (!net_.transitions()[transition].silent()) {
          visible_enabled[transition] = true;
          continue;
        }
        latest_marking[transition] = reached;
        enqueue(transition);
      }
    };
    discover(marking);

    // A token-producing silent cycle can generate infinitely many distinct
    // markings.  Bound diagnostic-only exploration just as the other silent
    // searches are bounded, while keeping finite self-loops terminating via
    // the transition-plus-marking visited key.
    std::size_t expanded = 0;
    while (!pending.empty()) {
      const auto transition = pending.front();
      pending.pop();
      const auto& latest = *latest_marking[transition];
      if (!visited[transition].insert(latest).second) continue;
      if (expanded++ >= kSilentSearchCap)
        throw std::runtime_error("enabled transition diagnostic search exceeded limit");
      Marking next = latest;
      fire_marking(next, transition);
      discover(next);
    }

    // The public helper returns a set. Keep the existing native diagnostic
    // ordering stable by emitting transition IDs in index order.
    std::vector<std::size_t> result;
    for (std::size_t t = 0; t < visible_enabled.size(); ++t)
      if (visible_enabled[t]) result.push_back(t);
    return result;
  }

 private:
  static TokenInteger arc_tokens(PetriArcWeight weight) {
    TokenInteger result(weight.magnitude); return weight.negative ? -result : result;
  }
  // Repeated-unit endpoints add up to one arc weight. Compact endpoints are
  // distinct arcs: like the reference, a place requires the weight of one arc
  // (the largest of parallel arcs) while firing still consumes every arc.
  static Marking input_requirements(const PetriTransition& transition, std::size_t places) {
    Marking result(places, 0);
    std::vector<bool> seen(transition.input_weights.empty() ? 0 : places, false);
    for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
      const auto place = transition.inputs[index];
      if (place >= places) throw std::invalid_argument("token replay arc place is outside marking");
      const auto weight = arc_tokens(transition.input_arc_weight(index));
      if (seen.empty()) result[place] += weight;
      else if (!seen[place] || result[place] < weight) result[place] = weight;
      if (!seen.empty()) seen[place] = true;
    }
    return result;
  }
  ForwardConfiguration prepare_forward(const TokenReplayOptions& options) const {
    ForwardConfiguration result;
    result.initial_keys = resolve_domain_marking_keys(options.initial_marking_keys, markings_.initial_keys, initial_, net_.place_count(), "initial_marking_keys");
    result.final_keys = resolve_domain_marking_keys(options.final_marking_keys, markings_.final_keys, final_, net_.place_count(), "final_marking_keys");
    for (const auto p : markings_.initial_order)
      if (result.initial_keys[p]) result.initial_order.push_back(p);
    for (std::size_t p = 0; p < result.initial_keys.size(); ++p)
      if (result.initial_keys[p] && std::find(result.initial_order.begin(), result.initial_order.end(), p) == result.initial_order.end())
        result.initial_order.push_back(p);
    result.place_names = detail::replay_place_names(options.place_names, net_);
    if (markings_.domain) for (const auto& place : markings_.domain->detached_places) {
      const auto* name = std::get_if<std::string>(&place.name.value);
      result.place_names.push_back(name ? *name : "detached:" + std::to_string(place.id));
    }
    result.place_name_rank = detail::name_ranks(result.place_names);
    result.transition_names = detail::replay_transition_names(
        options.nullable_transition_names, options.transition_names, net_);
    detail::validate_forward_transition_name_order(result.transition_names);
    if (options.cleaning_token_flood)
      result.components = detail::cleaning_components(net_, result.initial_keys, result.final_keys, &result.place_names);
    return result;
  }

  // A transition's input places in ascending order, each with its
  // input_requirements weight, built on first use for markings of `places`
  // slots (building throws as input_requirements does).
  struct FireInputs {
    std::size_t places = 0;
    std::vector<std::pair<std::size_t, TokenInteger>> required;
  };
  [[nodiscard]] const FireInputs& fire_inputs(std::size_t t, std::size_t places) const {
    auto& entry = fire_inputs_[t];
    if (entry && entry->places == places) return *entry;
    const auto& transition = net_.transitions()[t];
    const auto required = input_requirements(transition, places);
    std::vector<std::size_t> inputs(transition.inputs.begin(), transition.inputs.end());
    std::sort(inputs.begin(), inputs.end());
    inputs.erase(std::unique(inputs.begin(), inputs.end()), inputs.end());
    FireInputs built;
    built.places = places;
    for (const auto place : inputs) built.required.emplace_back(place, required[place]);
    entry = std::move(built);
    return *entry;
  }

  bool forward_fire(Marking& marking, detail::ReplayMarkingKeys& keys,
                    std::size_t transition, Counts& counts,
                    std::vector<std::size_t>* order = nullptr) const {
    counts.forward_replay = true;
    const auto& fired = net_.transitions()[transition];
    const auto& inputs = fire_inputs(transition, marking.size());
    const auto repair = [&](std::size_t place, const TokenInteger& weight) {
      if (marking[place] >= weight) return;
      const auto missing = TokenInteger(weight - marking[place]);
      counts.missing += missing;
      counts.places[place].missing += missing;
      // Public forward repair adds the full input weight before consumption,
      // retaining any partially available tokens after the firing.
      marking[place] += weight;
      if (order && !keys[place]) order->push_back(place);
      keys[place] = true;
    };
    if (fired.has_compact_ordinary_arcs()) {
      // Distinct arcs are repaired one after another, so parallel arcs on a
      // place deficient by more than one weight are each repaired in turn.
      for (std::size_t index = 0; index < fired.inputs.size(); ++index)
        repair(fired.inputs[index], arc_tokens(fired.input_arc_weight(index)));
    } else {
      for (const auto& [place, weight] : inputs.required) repair(place, weight);
    }
    // Signed input state can remain below its required weight even after the
    // public full-weight repair. Bookkeeping records the attempted transition,
    // but activation and token movement occur only when repair enables it.
    if (!enabled(marking, transition)) {
      for (std::size_t index = 0; index < fired.inputs.size(); ++index) {
        const auto weight = arc_tokens(fired.input_arc_weight(index));
        counts.consumed += weight; counts.places[fired.inputs[index]].consumed += weight;
      }
      for (std::size_t index = 0; index < fired.outputs.size(); ++index) {
        const auto weight = arc_tokens(fired.output_arc_weight(index));
        counts.produced += weight; counts.places[fired.outputs[index]].produced += weight;
      }
      return false;
    }
    if (order) {
      for (const auto& [place, weight] : inputs.required)
        if (marking[place] - weight == 0) std::erase(*order, place);
      for (const auto place : fired.reset_inputs) std::erase(*order, place);
      for (const auto place : fired.outputs)
        if (std::find(order->begin(), order->end(), place) == order->end())
          order->push_back(place);
    }
    fire(marking, transition, counts);
    for (const auto place : fired.inputs) keys[place] = marking[place] != 0;
    for (const auto place : fired.reset_inputs) keys[place] = marking[place] != 0;
    for (const auto place : fired.outputs) keys[place] = true;
    return true;
  }

  [[nodiscard]] static bool has_supplied_hidden_paths(const TokenReplayOptions& options) {
    if (!options.places_shortest_path_by_hidden) return false;
    const auto* value = std::get_if<EventAttribute>(&*options.places_shortest_path_by_hidden);
    return !value || !value->is_null;
  }

  [[nodiscard]] std::vector<detail::HiddenPlacePath> forward_place_paths(
      const detail::ReplayMarkingKeys& source_keys,
      const detail::ReplayMarkingKeys& target_keys,
      const std::vector<std::string>& place_names,
      const TokenReplayOptions& options,
      const std::vector<std::size_t>* place_name_rank = nullptr) const {
    if (!has_supplied_hidden_paths(options)) {
      // The structural search trees are the net's, not the marking's: grow
      // them once per replayer instead of once per routing query.
      const auto* order = options.place_outgoing_transition_order
                              ? &*options.place_outgoing_transition_order
                              : nullptr;
      if (!place_path_index_ ||
          !place_path_index_->matches(net_, source_keys.size(), 51, order))
        place_path_index_.emplace(net_, source_keys.size(), 51, order);
      return place_name_rank
                 ? place_path_index_->ranked_paths(source_keys, target_keys, *place_name_rank)
                 : place_path_index_->paths(source_keys, target_keys, place_names);
    }
    std::vector<detail::HiddenPlacePath> result;
    const auto* matrix = std::get_if<TokenReplayHiddenPathMatrix>(
        &*options.places_shortest_path_by_hidden);
    const auto* named = std::get_if<TokenReplayNamedHiddenPathMatrix>(
        &*options.places_shortest_path_by_hidden);
    if (named) {
      const auto transition_names = detail::replay_transition_names(
          options.nullable_transition_names, options.transition_names, net_);
      for (std::size_t source = 0; source < source_keys.size(); ++source) {
        if (!source_keys[source]) continue;
        const auto row = named->find(place_names[source]);
        if (row == named->end()) continue;
        for (std::size_t target = 0; target < target_keys.size(); ++target) {
          if (!target_keys[target]) continue;
          const auto path = row->second.find(place_names[target]);
          if (path == row->second.end()) continue;
          const auto unique_place = [&](std::size_t place) {
            return std::count(place_names.begin(), place_names.end(), place_names[place]) == 1;
          };
          if (!unique_place(source) || !unique_place(target))
            throw std::invalid_argument("hidden path cache contains an ambiguous place name");
          std::vector<std::size_t> transitions;
          transitions.reserve(path->second.size());
          for (const auto& name : path->second) {
            const auto found = std::find(transition_names.begin(), transition_names.end(), name);
            if (found == transition_names.end())
              throw std::invalid_argument("hidden path cache contains an invalid transition name");
            if (std::find(found + 1, transition_names.end(), name) != transition_names.end())
              throw std::invalid_argument("hidden path cache contains an ambiguous transition name");
            transitions.push_back(static_cast<std::size_t>(found - transition_names.begin()));
          }
          result.push_back({source, target, std::move(transitions)});
        }
      }
    } else if (!matrix) {
      // A native value carrier cannot contain model Place objects. Iterable
      // value containers therefore have no matching source keys. Non-iterable
      // scalars fail only when replay actually examines a marking key.
      const auto& value = std::get<EventAttribute>(*options.places_shortest_path_by_hidden);
      if (value.container != AttributeContainerKind::kScalar ||
          value.kind == AttributeKind::kString || value.kind == AttributeKind::kBytes)
        return result;
      if (std::any_of(source_keys.begin(), source_keys.end(), [](bool present) { return present; }))
        throw std::invalid_argument("hidden path cache is not iterable");
      return result;
    } else {
      for (std::size_t source = 0; source < source_keys.size(); ++source) {
        if (!source_keys[source]) continue;
        const auto row = matrix->find(source);
        if (row == matrix->end()) continue;
        for (std::size_t target = 0; target < target_keys.size(); ++target) {
          if (!target_keys[target]) continue;
          const auto path = row->second.find(target);
          if (path == row->second.end()) continue;
          for (const auto transition : path->second)
            if (transition >= net_.transitions().size())
              throw std::invalid_argument("hidden path cache contains an invalid transition index");
          result.push_back({source, target, path->second});
        }
      }
    }
    std::sort(result.begin(), result.end(), [&](const auto& left, const auto& right) {
      return std::tuple{left.transitions.size(), std::string_view(place_names[left.source]),
                        std::string_view(place_names[left.target]), left.source, left.target}
           < std::tuple{right.transitions.size(), std::string_view(place_names[right.source]),
                        std::string_view(place_names[right.target]), right.source, right.target};
    });
    return result;
  }

  template <class Emit>
  void forward_route_hidden(Marking& marking, detail::ReplayMarkingKeys& keys,
                            std::size_t target,
                            const std::vector<std::string>& place_names,
                            const std::vector<std::size_t>* place_name_rank,
                            const TokenReplayOptions& options,
                            const Emit& emit) const {
    // The input places still short of their requirement.
    const auto required = [&](std::size_t selected) {
      detail::ReplayMarkingKeys short_places(marking.size(), false);
      for (const auto& [place, weight] : fire_inputs(selected, marking.size()).required)
        if (!(marking[place] >= weight) && weight != 0) short_places[place] = true;
      return short_places;
    };
    const auto candidates = [&](std::size_t /*selected*/,
                                const detail::ReplayMarkingKeys& required_keys) {
      return forward_place_paths(keys, required_keys, place_names, options, place_name_rank);
    };
    const auto trial = [&](std::size_t selected,
                           const std::vector<detail::HiddenPlacePath>& paths,
                           const std::vector<bool>& visited) {
      const auto fire_trial = [&](Marking& trial_marking,
                                  detail::ReplayMarkingKeys& trial_keys,
                                  std::size_t transition) {
        Counts scratch;
        scratch.places.resize(trial_marking.size());
        // The transition is enabled, so forward_fire performs the same
        // checked marking/key update without contributing diagnostics or
        // usage counts to the real replay.
        forward_fire(trial_marking, trial_keys, transition, scratch);
      };
      return detail::exhaustive_hidden_transition_trial(
          selected, paths, visited, marking, keys,
          [&](const Marking& candidate, std::size_t transition) {
            return enabled(candidate, transition);
          },
          fire_trial);
    };
    detail::route_hidden_dependencies(net_.transitions().size(), target,
        options.exhaustive_invisible_exploration,
        candidates, required,
        [&](auto transition) { return enabled(marking, transition); }, emit,
        trial);
  }

  // One precision/clean-run search step's outcome — its marking and token
  // bookkeeping it added. Replay is a deterministic function of the marking
  // and the activity, and real logs repeat the same steps constantly (shared
  // variant prefixes, recurring states), so each distinct step is computed
  // (with its silent-routing search) once and replayed from the cache after
  // that. These caches support prefix precision and clean-run searches; the
  // forward diagnostics path also retains incomplete routing and cleaning.
  // Only what the cache readers use is kept: per-place diagnostics would
  // cost four token counts per place for every cached step.
  struct Step {
    Marking marking;
    TokenInteger missing = 0;
    std::vector<std::size_t> fired;  // transitions fired, silent routing included
    bool capped = false;  // a silent-routing search stopped at its bound
  };

  struct StepKeyHash {
    std::size_t operator()(
        const std::pair<Marking, std::size_t>& key) const noexcept {
      return MarkingHash{}(key.first) ^ (key.second * 0x9e3779b97f4a7c15ull);
    }
  };

  [[nodiscard]] const Step& cached_step(const Marking& marking,
                                        std::size_t activity,
                                        bool walk_through_hidden_transitions = true) const {
    auto& cache = walk_through_hidden_transitions ? step_cache_ : direct_step_cache_;
    const auto [it, inserted] =
        cache.try_emplace(std::make_pair(marking, activity));
    if (inserted) {
      Step& step = it->second;
      step.marking = marking;
      Counts counts;
      const auto& targets = transitions_by_activity_[activity];
      const auto capped_before = capped_searches_;
      if (walk_through_hidden_transitions)
        route_silently(step.marking, targets, counts, &step.fired);
      step.capped = capped_searches_ != capped_before;
      const std::size_t chosen = pick_enabled(step.marking, targets);
      fire(step.marking, chosen, counts);
      step.fired.push_back(chosen);
      step.missing = counts.missing;
    }
    return it->second;
  }

  // The end-of-trace routing toward the requested final marking, cached like
  // an ordinary replay step.
  [[nodiscard]] const Step& cached_finish(const Marking& marking) const {
    const auto [it, inserted] = finish_cache_.try_emplace(marking);
    if (inserted) {
      Step& step = it->second;
      step.marking = marking;
      Counts counts;
      const auto capped_before = capped_searches_;
      route_silently_to_marking(step.marking, counts, &step.fired);
      step.capped = capped_searches_ != capped_before;
      step.missing = counts.missing;
    }
    return it->second;
  }

 public:
  // Cached steps across the three step caches, and a way to drop them. The
  // caches are pure memoization, so clearing them never changes a result.
  [[nodiscard]] std::size_t cached_steps() const noexcept {
    return step_cache_.size() + direct_step_cache_.size() + finish_cache_.size();
  }
  void clear_step_caches() const {
    step_cache_ = {};
    direct_step_cache_ = {};
    finish_cache_ = {};
  }

 private:

  // Detects whether the exact per-place closure applies (see
  // enabled_activities) and precomputes, for every place, which activities a
  // single token there can eventually enable through silent firings.
  void build_place_closure() {
    for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
      const auto& transition = net_.transitions()[t];
      const bool ok = !transition.has_extended_arcs() && !transition.has_compact_ordinary_arcs() &&
                      (transition.silent()
                          ? transition.inputs.size() == 1 &&
                                transition.outputs.size() == 1
                          : transition.inputs.size() == 1);
      if (!ok) return;  // fall back to the breadth-first closure
    }

    const std::size_t places = initial_.size();
    std::vector<std::vector<std::size_t>> silent_out(places);
    for (const std::size_t s : silent_transitions_) {
      silent_out[net_.transitions()[s].inputs.front()].push_back(
          net_.transitions()[s].outputs.front());
    }
    std::vector<std::vector<std::size_t>> enables_at(places);
    for (std::size_t a = 0; a < transitions_by_activity_.size(); ++a) {
      for (const std::size_t t : transitions_by_activity_[a]) {
        enables_at[net_.transitions()[t].inputs.front()].push_back(a);
      }
    }

    place_enables_.resize(places);
    std::vector<char> seen(places);
    std::vector<std::size_t> stack;
    std::vector<char> acts(transitions_by_activity_.size());
    for (std::size_t p = 0; p < places; ++p) {
      std::fill(seen.begin(), seen.end(), 0);
      std::fill(acts.begin(), acts.end(), 0);
      seen[p] = 1;
      stack.assign(1, p);
      while (!stack.empty()) {
        const std::size_t q = stack.back();
        stack.pop_back();
        for (const std::size_t a : enables_at[q]) acts[a] = 1;
        for (const std::size_t r : silent_out[q]) {
          if (!seen[r]) {
            seen[r] = 1;
            stack.push_back(r);
          }
        }
      }
      for (std::size_t a = 0; a < acts.size(); ++a) {
        if (acts[a]) place_enables_[p].push_back(a);
      }
    }
    place_closure_exact_ = true;
  }

  // The places a token could ever occupy from `marking` through silent
  // firings when token counts are ignored — a cheap over-approximation of the
  // silent closure. Sound by induction: a real firing needs every input place
  // tokened, so every place a real silent run can mark lands in the fixpoint.
  // What the fixpoint rules out is therefore truly unreachable, which lets
  // the exact (and expensive) searches skip provably hopeless goals.
  [[nodiscard]] std::vector<char> relaxed_silent_places(
      const Marking& marking) const {
    std::vector<char> reachable(marking.size(), 0);
    for (std::size_t p = 0; p < marking.size(); ++p) {
      reachable[p] = marking[p] > 0 ? 1 : 0;
    }
    bool changed = true;
    while (changed) {
      changed = false;
      for (const std::size_t s : silent_transitions_) {
        if (!relaxed_enables(reachable, s)) continue;
        for (const std::size_t place : net_.transitions()[s].outputs) {
          if (!reachable[place]) {
            reachable[place] = 1;
            changed = true;
          }
        }
      }
    }
    return reachable;
  }

  [[nodiscard]] bool relaxed_enables(const std::vector<char>& reachable,
                                     std::size_t t) const {
    const auto& transition = net_.transitions()[t];
    for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
      if (arc_tokens(transition.input_arc_weight(index)) <= 0) continue;
      if (!reachable[transition.inputs[index]]) return false;
    }
    return true;
  }

  [[nodiscard]] bool enabled(const Marking& marking, std::size_t t) const {
    const auto& transition = net_.transitions()[t];
    const auto& checks = input_checks(t, marking.size());
    if (checks.per_call) {
      for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
        const auto place = transition.inputs[index];
        if (place >= marking.size() || marking[place] < arc_tokens(transition.input_arc_weight(index))) return false;
      }
    }
    if (checks.never) return false;
    for (const auto& check : checks.checks)
      if (marking[check.place] < check.required) return false;
    for (const auto place : transition.inhibitor_inputs)
      if (place >= marking.size() || marking[place] != 0) return false;
    return true;
  }

  // The enabling thresholds of transition t for markings of `places` slots,
  // built on first use. Compact endpoints are checked arc by arc (an arc
  // outside the marking disables the transition); repeated-unit endpoints
  // need input_requirements per place. Searches call enabled() for every
  // silent transition at every expanded marking, so rebuilding a dense
  // requirement marking per call dominated replays of large logs.
  [[nodiscard]] const InputChecks& input_checks(std::size_t t, std::size_t places) const {
    auto& entry = input_checks_[t];
    if (entry && entry->places == places) return *entry;
    const auto& transition = net_.transitions()[t];
    InputChecks built;
    built.places = places;
    if (transition.has_compact_ordinary_arcs()) {
      try {
        for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
          const auto place = transition.inputs[index];
          if (place >= places) { built.never = true; built.checks.clear(); break; }
          built.checks.push_back({place, arc_tokens(transition.input_arc_weight(index))});
        }
      } catch (const std::invalid_argument&) {
        // Whether the invalid weight is reached depends on the marking.
        built = InputChecks{};
        built.places = places;
        built.per_call = true;
      }
    } else {
      const auto required = input_requirements(transition, places);  // throws as before
      for (const auto place : transition.inputs) built.checks.push_back({place, required[place]});
    }
    entry = std::move(built);
    return *entry;
  }

  // The marking effect of fire() without its bookkeeping, for searches that
  // throw the counts away: inputs are consumed and clamped at zero (the
  // default backwards firing), reset inputs emptied, outputs produced.
  void fire_marking(Marking& marking, std::size_t t) const {
    const auto& transition = net_.transitions()[t];
    auto& cached = arc_weights_[t];
    if (!cached) {
      // Built in fire()'s order, so an invalid weight throws as it does.
      ArcWeights built;
      built.inputs.reserve(transition.inputs.size());
      for (std::size_t index = 0; index < transition.inputs.size(); ++index)
        built.inputs.push_back(arc_tokens(transition.input_arc_weight(index)));
      built.outputs.reserve(transition.outputs.size());
      for (std::size_t index = 0; index < transition.outputs.size(); ++index)
        built.outputs.push_back(arc_tokens(transition.output_arc_weight(index)));
      cached = std::move(built);
    }
    const auto& weights = *cached;
    for (std::size_t index = 0; index < transition.inputs.size(); ++index)
      marking[transition.inputs[index]] -= weights.inputs[index];
    for (const auto place : transition.inputs)
      if (marking[place] < 0) marking[place] = 0;
    for (const std::size_t place : transition.reset_inputs)
      if (marking[place] > 0) marking[place] = 0;
    for (std::size_t index = 0; index < transition.outputs.size(); ++index)
      marking[transition.outputs[index]] += weights.outputs[index];
  }

  // The reference keys a transition's preset by place, so of several
  // physically parallel input arcs from one place only the last one's weight
  // decides whether a backwards weak firing still counts as fitting.
  [[nodiscard]] bool preset_satisfied(const Marking& marking, std::size_t t) const {
    const auto& transition = net_.transitions()[t];
    std::vector<std::optional<TokenInteger>> last(marking.size());
    for (std::size_t index = 0; index < transition.inputs.size(); ++index)
      last[transition.inputs[index]] = arc_tokens(transition.input_arc_weight(index));
    for (std::size_t place = 0; place < last.size(); ++place)
      if (last[place] && marking[place] < *last[place]) return false;
    return true;
  }

  [[nodiscard]] bool any_enabled(const Marking& marking,
                                 const std::vector<std::size_t>& ts) const {
    return std::any_of(ts.begin(), ts.end(),
                       [&](std::size_t t) { return enabled(marking, t); });
  }

  [[nodiscard]] std::size_t pick_enabled(
      const Marking& marking, const std::vector<std::size_t>& targets) const {
    for (const std::size_t t : targets) {
      if (enabled(marking, t)) return t;
    }
    return targets.front();  // forced firing: missing tokens are counted in fire()
  }

  void fire(Marking& marking, std::size_t t, Counts& counts) const {
    if (counts.places.size() < marking.size())
      counts.places.resize(marking.size());
    const auto& transition = net_.transitions()[t];
    if (counts.forward_replay) {
      for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
        const auto place = transition.inputs[index];
        const auto weight = arc_tokens(transition.input_arc_weight(index));
        marking[place] -= weight;
        counts.consumed += weight; counts.places[place].consumed += weight;
      }
    } else {
      // Backwards replay: every input arc is checked against the marking
      // before the firing, so physically parallel arcs from one place each
      // report their own shortfall; the place then loses all their tokens and
      // never drops below zero.
      for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
        const auto place = transition.inputs[index];
        const auto weight = arc_tokens(transition.input_arc_weight(index));
        if (marking[place] < weight) {
          const auto missing = weight - marking[place];
          counts.missing += missing; counts.places[place].missing += missing;
        }
      }
      for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
        const auto place = transition.inputs[index];
        const auto weight = arc_tokens(transition.input_arc_weight(index));
        marking[place] -= weight;
        counts.consumed += weight; counts.places[place].consumed += weight;
      }
      for (const auto place : transition.inputs)
        if (marking[place] < 0) marking[place] = 0;
    }
    for (const std::size_t place : transition.reset_inputs) {
      if (marking[place] > 0) {
        const auto value = TokenInteger(marking[place]);
        counts.consumed += value;
        counts.places[place].consumed += value;
        marking[place] = 0;
      }
    }
    for (std::size_t index = 0; index < transition.outputs.size(); ++index) {
      const auto place = transition.outputs[index];
      const auto weight = arc_tokens(transition.output_arc_weight(index));
      marking[place] += weight;
      counts.produced += weight; counts.places[place].produced += weight;
    }
  }

  static void add_place_counts(
      std::vector<TokenReplayPlaceDiagnostics>& target,
      const std::vector<TokenReplayPlaceDiagnostics>& source) {
    if (target.size() < source.size()) target.resize(source.size());
    for (std::size_t place = 0; place < source.size(); ++place) {
      target[place].missing += source[place].missing;
      target[place].remaining += source[place].remaining;
      target[place].consumed += source[place].consumed;
      target[place].produced += source[place].produced;
    }
  }

  // --- Flat silent search ---------------------------------------------------
  // silent_path over markings stored as rows of 32-bit counts in one pool,
  // deduplicated by an open-addressing table of row ids. It visits the same
  // markings in the same order as silent_path — the frontier is FIFO, the
  // silent transitions are tried in silent_transitions_ order, the cap counts
  // the same expansions — so it returns the same path. It declines (returns
  // kFlatDeclined) whenever a count, weight or threshold is outside what the
  // rows hold, or an arc weight is invalid; the caller then runs silent_path.
  struct FlatTransition {
    bool ready = false;
    bool usable = false;  // false: an invalid weight or per-call check
    bool never = false;   // an input arc lies outside the marking
    std::vector<std::pair<std::uint32_t, std::int64_t>> checks;  // place, required
    std::vector<std::uint32_t> inhibitors;
    std::vector<std::pair<std::uint32_t, std::int64_t>> inputs;   // consumed, arc order
    std::vector<std::uint32_t> resets;
    std::vector<std::pair<std::uint32_t, std::int64_t>> outputs;  // produced, arc order
  };

  // A built entry the flat search cannot use.
  static FlatTransition declined_flat() {
    FlatTransition entry;
    entry.ready = true;
    return entry;
  }

  static std::int64_t clamp_threshold(const TokenInteger& value) {
    if (value.is_small()) return value.small_value();
    return value > 0 ? std::numeric_limits<std::int64_t>::max()
                     : std::numeric_limits<std::int64_t>::min();
  }

  [[nodiscard]] const FlatTransition& flat_transition(std::size_t t, std::size_t places) const {
    if (flat_places_ != places) {
      flat_transitions_.assign(net_.transitions().size(), FlatTransition{});
      flat_places_ = places;
    }
    auto& entry = flat_transitions_[t];
    if (entry.ready) return entry;
    entry.ready = true;
    const auto& transition = net_.transitions()[t];
    FlatTransition built;
    built.ready = true;
    try {
      const auto& checks = input_checks(t, places);
      if (checks.per_call) { entry = std::move(built); return entry; }
      built.never = checks.never;
      for (const auto& check : checks.checks)
        built.checks.emplace_back(static_cast<std::uint32_t>(check.place), clamp_threshold(check.required));
      for (const auto place : transition.inhibitor_inputs) {
        if (place >= places) built.never = true;
        else built.inhibitors.push_back(static_cast<std::uint32_t>(place));
      }
      // fire_marking's weights, in its order.
      for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
        const auto weight = arc_tokens(transition.input_arc_weight(index));
        if (!weight.is_small() || transition.inputs[index] >= places) { entry = declined_flat(); return entry; }
        built.inputs.emplace_back(static_cast<std::uint32_t>(transition.inputs[index]), weight.small_value());
      }
      for (const auto place : transition.reset_inputs) {
        if (place >= places) { entry = declined_flat(); return entry; }
        built.resets.push_back(static_cast<std::uint32_t>(place));
      }
      for (std::size_t index = 0; index < transition.outputs.size(); ++index) {
        const auto weight = arc_tokens(transition.output_arc_weight(index));
        if (!weight.is_small() || transition.outputs[index] >= places) { entry = declined_flat(); return entry; }
        built.outputs.emplace_back(static_cast<std::uint32_t>(transition.outputs[index]), weight.small_value());
      }
    } catch (const std::exception&) {
      entry = declined_flat();  // the generic search reproduces the failure
      return entry;
    }
    built.usable = true;
    entry = std::move(built);
    return entry;
  }

  template <class Cell>
  static bool flat_enabled(const FlatTransition& transition, const Cell* row) noexcept {
    if (transition.never) return false;
    for (const auto& [place, required] : transition.checks)
      if (row[place] < required) return false;
    for (const auto place : transition.inhibitors)
      if (row[place] != 0) return false;
    return true;
  }

  // Applies fire_marking to `row`; false when a count leaves the range of
  // the row's cells (32-bit, or 8-bit in the forward enabled search).
  template <class Cell>
  static bool flat_fire(const FlatTransition& transition, Cell* row) noexcept {
    constexpr std::int64_t lo = std::numeric_limits<Cell>::min();
    constexpr std::int64_t hi = std::numeric_limits<Cell>::max();
    for (const auto& [place, weight] : transition.inputs) {
      const std::int64_t value = std::int64_t(row[place]) - weight;
      if (value < lo || value > hi) return false;
      row[place] = static_cast<Cell>(value);
    }
    for (const auto& input : transition.inputs)
      if (row[input.first] < 0) row[input.first] = 0;
    for (const auto place : transition.resets)
      if (row[place] > 0) row[place] = 0;
    for (const auto& [place, weight] : transition.outputs) {
      const std::int64_t value = std::int64_t(row[place]) + weight;
      if (value < lo || value > hi) return false;
      row[place] = static_cast<Cell>(value);
    }
    return true;
  }

  // enabled_transition_ids' forward worklist on 32-bit place rows, with the
  // same outcome as that FIFO of (transition, latest marking) entries: the
  // same visited (name, label, marking) triples, expansions in the same
  // order, the same caps and exceptions, and `selected` in the same order.
  //
  // The FIFO is not materialised entry by entry. Each discovery of a marking
  // appends one batch: the marking's enabled transitions in `order`. A
  // popped entry of t acts on latest[t], the marking of the newest batch
  // holding t, and leaves (pair of t, latest[t]) visited whether or not it
  // already was. So once an entry of t is popped, every later entry of t
  // pops a visited pair, and does nothing, until a newer batch holds t
  // again. Only the first entry of t after such a rediscovery (t "dirty")
  // can act; those entries are processed in FIFO order (batch, then
  // position) and the others are passed over. A batch holds t exactly when
  // its marking enables t, so the batches holding t are found from the
  // markings' enabled sets (a row per marking, and, for scans that run long,
  // a cached word per complete block of 64 batches and transition). Markings
  // are interned in a flat pool; a successor's enabled set and row hash
  // follow from its parent's and the places the firing touched. False, with
  // `selected` unspecified, when a transition or a count does not fit the
  // flat form; the generic search then runs from the start and, being the
  // same deterministic walk, reaches the same outcome.
  [[nodiscard]] bool flat_forward_enabled(const Marking& marking, ForwardEnabledPlan& plan,
                                          std::optional<std::size_t> budget,
                                          std::size_t worklist_cap,
                                          std::vector<std::size_t>& selected) const {
    const std::size_t places = marking.size();
    const auto& order = plan.order;
    const std::size_t transition_count = order.size();
    if (transition_count == 0 || transition_count != net_.transitions().size() ||
        transition_count >= std::numeric_limits<std::uint32_t>::max() ||
        places >= std::numeric_limits<std::uint32_t>::max())
      return false;
    auto& s = flat_enabled_scratch_;
    s.by_transition.resize(transition_count);
    for (std::size_t t = 0; t < transition_count; ++t) {
      const auto& entry = flat_transition(t, places);
      if (!entry.usable) return false;
      s.by_transition[t] = &entry;
    }
    s.flat.resize(transition_count);
    for (std::size_t position = 0; position < transition_count; ++position)
      s.flat[position] = s.by_transition[order[position]];
    if (plan.flat_places != places) {
      plan.flat_places = places;
      plan.flat_readers.assign(places, {});
      plan.flat_touched.assign(transition_count, {});
      plan.flat_pair.resize(transition_count);
      plan.flat_silent.resize(transition_count);
      for (std::size_t position = 0; position < transition_count; ++position) {
        const auto& transition = *s.flat[position];
        plan.flat_pair[position] = static_cast<std::uint32_t>(plan.pair_ids[order[position]]);
        plan.flat_silent[position] = net_.transitions()[order[position]].silent() ? 1 : 0;
        auto& touched = plan.flat_touched[position];
        for (const auto& input : transition.inputs) touched.push_back(input.first);
        for (const auto place : transition.resets) touched.push_back(place);
        for (const auto& output : transition.outputs) touched.push_back(output.first);
        std::sort(touched.begin(), touched.end());
        touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
        // A transition that is never enabled reads nothing that can change it.
        if (transition.never) continue;
        std::vector<std::uint32_t> read;
        for (const auto& check : transition.checks) read.push_back(check.first);
        for (const auto place : transition.inhibitors) read.push_back(place);
        std::sort(read.begin(), read.end());
        read.erase(std::unique(read.begin(), read.end()), read.end());
        for (const auto place : read)
          plan.flat_readers[place].push_back(static_cast<std::uint32_t>(position));
      }
    }
    // Rows of 8-bit cells while every count fits them; a search whose counts
    // outgrow them is repeated with 32-bit cells, from the start, as the
    // same deterministic walk.
    bool small = true;
    for (std::size_t place = 0; place < places; ++place) {
      const auto& value = marking[place];
      if (!value.is_small() || value.small_value() < std::numeric_limits<std::int32_t>::min() ||
          value.small_value() > std::numeric_limits<std::int32_t>::max())
        return false;
      small = small && value.small_value() >= std::numeric_limits<std::int8_t>::min() &&
              value.small_value() <= std::numeric_limits<std::int8_t>::max();
    }
    if (small) {
      const auto outcome = flat_forward_search<std::int8_t>(marking, plan, budget, worklist_cap, selected);
      if (outcome != FlatSearch::kOverflow) return outcome == FlatSearch::kDone;
      selected.clear();
    }
    return flat_forward_search<std::int32_t>(marking, plan, budget, worklist_cap, selected) ==
           FlatSearch::kDone;
  }

  enum class FlatSearch { kDone, kDeclined, kOverflow };
  // flat_forward_enabled's search on rows of `Cell` counts. kOverflow when a
  // count leaves the range of 8-bit cells (32-bit ones decline instead).
  template <class Cell>
  [[nodiscard]] FlatSearch flat_forward_search(const Marking& marking, ForwardEnabledPlan& plan,
                                               std::optional<std::size_t> budget,
                                               std::size_t worklist_cap,
                                               std::vector<std::size_t>& selected) const {
    constexpr FlatSearch kOutOfRange =
        sizeof(Cell) < sizeof(std::int32_t) ? FlatSearch::kOverflow : FlatSearch::kDeclined;
    const std::size_t places = marking.size();
    const auto& order = plan.order;
    const std::size_t transition_count = order.size();
    auto& s = flat_enabled_scratch_;
    const auto& flat = s.flat;
    const auto& readers = plan.flat_readers;
    const auto& touched_places = plan.flat_touched;
    const auto& pair_of = plan.flat_pair;
    const auto& silent_at = plan.flat_silent;

    // Buffers grow geometrically and are reused across searches; `rows`,
    // `batches` and the logical sizes below say how much of each is in use.
    const auto reserve = [](auto& buffer, std::size_t size) {
      if (buffer.size() < size) buffer.resize(std::max(size, buffer.size() * 2));
    };
    auto& pool = [&]() -> std::vector<Cell>& {
      if constexpr (sizeof(Cell) < sizeof(std::int32_t)) return s.small_pool;
      else return s.pool;
    }();
    reserve(pool, places);
    for (std::size_t place = 0; place < places; ++place) {
      const auto& value = marking[place];
      if (!value.is_small() || value.small_value() < std::numeric_limits<Cell>::min() ||
          value.small_value() > std::numeric_limits<Cell>::max())
        return kOutOfRange;
      pool[place] = static_cast<Cell>(value.small_value());
    }
    const std::size_t words = (transition_count + 63) / 64;
    const std::size_t pair_words = (plan.pair_count + 63) / 64;
    auto& hashes = s.hashes;                // by marking id; rows are pool[id * places, ...)
    auto& visited = s.visited;              // by marking id: a bit per (name, label) pair
    auto& enabled_rows = s.enabled;         // by marking id: a bit per enabled position
    auto& enabled_count = s.enabled_count;  // by marking id
    auto& batch_marking = s.batch_marking;  // by batch: the discovered marking id
    auto& batch_start = s.batch_start;      // by batch: FIFO entries before it
    auto& batch_rows = s.batch_rows;        // by batch: its marking's enabled set
    auto& live = s.live;                    // by batch: positions whose entry may act
    auto& live_any = s.live_any;            // a bit per batch: its live row is not empty
    auto& dirty = s.dirty;                  // positions with a live entry
    auto& block_column = s.block_column;    // by block: offset of its columns, or none
    auto& columns = s.columns;              // by built block: a word per position
    constexpr std::size_t kNoColumns = std::numeric_limits<std::size_t>::max();
    // Columns are an index only; past this many words scans read rows.
    constexpr std::size_t kColumnWordLimit = std::size_t{1} << 21;
    std::size_t rows = 0, batches = 0, column_words = 0;
    block_column.clear();
    dirty.assign(words, 0);
    constexpr std::uint32_t kEmpty = std::numeric_limits<std::uint32_t>::max();
    // The table is reused across searches: only the slots the last search
    // filled are cleared (`table_used`), so a search costs its own rows, not
    // the largest table an earlier search grew. A table past 2^16 slots is
    // given back once.
    auto& table = s.table;
    auto& table_used = s.table_used;
    if (table.size() < 256 || table.size() > (std::size_t{1} << 16)) {
      table.assign(table.size() < 256 ? 256 : std::size_t{1} << 16, kEmpty);
      table.shrink_to_fit();
    } else {
      for (const auto slot : table_used) table[slot] = kEmpty;
    }
    table_used.clear();

    // Row hashes are additive over places, so a successor's hash follows from
    // its parent's and the places the firing touched. They only bucket rows
    // (equality is checked in full), so which rows are equal is unchanged.
    const auto mix = [](std::size_t place, std::int64_t value) {
      std::uint64_t x = (std::uint64_t(place) << 32) ^ static_cast<std::uint32_t>(value);
      x += 0x9e3779b97f4a7c15ull;
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
      return x ^ (x >> 31);
    };
    // Interns the row in pool slot `rows`: its id and whether it is new (an
    // earlier row's id when the marking was reached before).
    const auto intern_next = [&](std::uint64_t h) -> std::pair<std::uint32_t, bool> {
      const auto id = static_cast<std::uint32_t>(rows);
      if ((rows + 1) * 2 > table.size()) {
        auto& grown = s.grown;
        grown.assign(table.size() * 2, kEmpty);
        const std::size_t mask = grown.size() - 1;
        for (auto& used : table_used) {
          const auto existing = table[used];
          std::size_t slot = hashes[existing] & mask;
          while (grown[slot] != kEmpty) slot = (slot + 1) & mask;
          grown[slot] = existing;
          used = slot;
        }
        table.swap(grown);
      }
      const std::size_t mask = table.size() - 1;
      std::size_t slot = h & mask;
      const Cell* row = pool.data() + std::size_t(id) * places;
      while (table[slot] != kEmpty) {
        const auto other = table[slot];
        if (hashes[other] == h &&
            std::equal(row, row + places, pool.data() + std::size_t(other) * places))
          return {other, false};
        slot = (slot + 1) & mask;
      }
      table[slot] = id;
      table_used.push_back(slot);
      ++rows;
      if (hashes.size() < rows) {
        reserve(hashes, rows);
        reserve(visited, rows * pair_words);
        reserve(enabled_rows, rows * words);
        reserve(enabled_count, rows);
      }
      hashes[id] = h;
      std::fill_n(visited.data() + std::size_t(id) * pair_words, pair_words, 0);
      return {id, true};
    };

    const auto holds = [&](std::size_t batch, std::size_t position) {
      return (batch_rows.data()[batch * words + position / 64] >> (position % 64)) & 1;
    };
    // Whether the batches of complete block `block` holding `position` are
    // known as a column word (`word`). Words are computed on first use, when
    // a scan has already read `scanned` rows without a hit: the batches
    // holding a transition are usually a few rows apart, and a scan that runs
    // long tends to cross the same blocks again for the same transition.
    const auto column_word = [&](std::size_t block, std::size_t position, std::size_t scanned,
                                 std::uint64_t& word) {
      if (block_column.size() <= block) block_column.resize(block + 1, kNoColumns);
      if (block_column[block] == kNoColumns) {
        if (scanned < 64 || column_words + transition_count + words > kColumnWordLimit) return false;
        reserve(columns, column_words + transition_count + words);
        std::fill_n(columns.data() + column_words, transition_count + words, 0);
        block_column[block] = column_words;
        column_words += transition_count + words;
      }
      std::uint64_t* column = columns.data() + block_column[block];
      std::uint64_t* known = column + transition_count;
      const std::uint64_t bit = std::uint64_t{1} << (position % 64);
      if ((known[position / 64] & bit) == 0) {
        if (scanned < 64) return false;
        std::uint64_t value = 0;
        for (std::size_t r = 0; r < 64; ++r)
          value |= std::uint64_t(holds(block * 64 + r, position)) << r;
        column[position] = value;
        known[position / 64] |= bit;
      }
      word = column[position];
      return true;
    };
    const auto has_columns = [&](std::size_t block) {
      return block < block_column.size() && block_column[block] != kNoColumns;
    };
    // The first batch from `batch` on that holds `position` (one does).
    const auto first_holding = [&](std::size_t batch, std::size_t position) {
      const std::size_t offset = position / 64;
      const std::uint64_t bit = std::uint64_t{1} << (position % 64);
      for (std::size_t scanned = 0;;) {
        // Rows first; a block's column word once the scan has run long.
        const std::size_t block = batch / 64;
        std::uint64_t word = 0;
        if ((scanned >= 64 || has_columns(block)) && block * 64 + 64 <= batches &&
            column_word(block, position, scanned, word)) {
          word &= ~std::uint64_t{0} << (batch % 64);
          if (word != 0) return block * 64 + static_cast<std::size_t>(std::countr_zero(word));
          batch = block * 64 + 64;
          continue;
        }
        const std::uint64_t* held = batch_rows.data() + offset;
        const std::size_t end = std::min(block * 64 + 64, batches);
        for (; batch < end; ++batch, ++scanned)
          if (held[batch * words] & bit) return batch;
      }
    };
    // The newest batch that holds `position` (one does).
    const auto last_holding = [&](std::size_t position) {
      const std::size_t offset = position / 64;
      const std::uint64_t bit = std::uint64_t{1} << (position % 64);
      std::size_t batch = batches;  // one past the next batch to read
      for (std::size_t scanned = 0;;) {
        const std::size_t block = (batch - 1) / 64;
        std::uint64_t word = 0;
        if ((scanned >= 64 || has_columns(block)) && block * 64 + 64 <= batches &&
            column_word(block, position, scanned, word)) {
          const auto last = (batch - 1) % 64;
          word &= last == 63 ? ~std::uint64_t{0} : (std::uint64_t{1} << (last + 1)) - 1;
          if (word != 0) return block * 64 + static_cast<std::size_t>(std::bit_width(word)) - 1;
          batch = block * 64;
          continue;
        }
        const std::uint64_t* held = batch_rows.data() + offset;
        for (; batch > block * 64; ++scanned)
          if (held[--batch * words] & bit) return batch;
      }
    };

    // The FIFO head: the entry being processed, as (batch, position);
    // position -1 before the first entry.
    std::size_t head_batch = 0;
    std::ptrdiff_t head_position = -1;
    std::size_t pushed = 0;  // FIFO entries appended so far
    // Appends the batch of marking `id`: the reference's discover().
    const auto discover = [&](std::uint32_t id) {
      const std::size_t count = enabled_count[id];
      if (count != 0) {
        // The reference checks the queued entries before each push; at most
        // the entries from the head's batch on are queued.
        const std::size_t before = head_position >= 0 ? batch_start[head_batch] : 0;
        if ((pushed - before) + (count - 1) >= worklist_cap) {
          std::size_t popped = before;
          if (head_position >= 0) {
            const std::uint64_t* row = enabled_rows.data() + std::size_t(batch_marking[head_batch]) * words;
            const auto last = static_cast<std::size_t>(head_position);
            for (std::size_t word = 0; word < last / 64; ++word) popped += std::popcount(row[word]);
            const auto bit = last % 64;
            popped += std::popcount(row[last / 64] &
                                    (bit == 63 ? ~std::uint64_t{0} : (std::uint64_t{1} << (bit + 1)) - 1));
          }
          if ((pushed - popped) + (count - 1) >= worklist_cap)
            throw std::runtime_error("enabled transition diagnostic worklist exceeded limit");
        }
      }
      if (batch_marking.size() <= batches) {
        reserve(batch_marking, batches + 1);
        reserve(batch_start, batches + 1);
        reserve(batch_rows, (batches + 1) * words);
        reserve(live, (batches + 1) * words);
        reserve(live_any, batches / 64 + 1);
      }
      batch_marking[batches] = id;
      batch_start[batches] = pushed;
      pushed += count;
      ++batches;
      std::fill_n(live.data() + (batches - 1) * words, words, 0);
      if ((batches - 1) % 64 == 0) live_any[(batches - 1) / 64] = 0;
      const std::uint64_t* enabled = enabled_rows.data() + std::size_t(id) * words;
      std::copy_n(enabled, words, batch_rows.data() + (batches - 1) * words);
      for (std::size_t word = 0; word < words; ++word) {
        const std::uint64_t fresh = enabled[word] & ~dirty[word];
        if (fresh == 0) continue;
        dirty[word] |= fresh;
        for (auto bits = fresh; bits != 0; bits &= bits - 1) {
          const auto position = word * 64 + static_cast<std::size_t>(std::countr_zero(bits));
          // Its first entry after the head: in a batch after the head's, or
          // in the head's batch itself when it comes after the head there.
          const std::size_t from =
              static_cast<std::ptrdiff_t>(position) > head_position ? head_batch : head_batch + 1;
          const auto batch = first_holding(from, position);
          live[batch * words + word] |= std::uint64_t{1} << (position % 64);
          live_any[batch / 64] |= std::uint64_t{1} << (batch % 64);
        }
      }
    };
    {
      std::uint64_t h = 0;
      for (std::size_t place = 0; place < places; ++place) h += mix(place, pool[place]);
      const auto [id, added] = intern_next(h);
      (void)added;
      std::uint64_t* enabled = enabled_rows.data() + std::size_t(id) * words;
      std::fill_n(enabled, words, 0);
      std::size_t count = 0;
      const Cell* row = pool.data();
      for (std::size_t position = 0; position < transition_count; ++position)
        if (flat_enabled(*flat[position], row)) {
          enabled[position / 64] |= std::uint64_t{1} << (position % 64);
          ++count;
        }
      enabled_count[id] = static_cast<std::uint32_t>(count);
      discover(id);
    }

    std::size_t expanded = 0;
    while (true) {
      // The next live entry in FIFO order: later in the head's batch, or the
      // first in the next batch with a live row.
      std::size_t position = 0;
      {
        std::size_t batch = head_batch;
        const auto first_in = [&](std::size_t row_batch, std::size_t from) {
          const std::uint64_t* row = live.data() + row_batch * words;
          std::uint64_t mask = ~std::uint64_t{0} << (from % 64);
          for (std::size_t word = from / 64; word < words; ++word, mask = ~std::uint64_t{0})
            if (const std::uint64_t bits = row[word] & mask; bits != 0)
              return word * 64 + static_cast<std::size_t>(std::countr_zero(bits));
          return transition_count;
        };
        position = batch < batches ? first_in(batch, static_cast<std::size_t>(head_position + 1))
                                   : transition_count;
        if (position == transition_count) {
          std::size_t next = batch + 1;
          bool found = false;
          for (std::size_t group = next / 64; group * 64 < batches; ++group) {
            std::uint64_t bits = live_any[group];
            if (group == next / 64) bits &= ~std::uint64_t{0} << (next % 64);
            if (bits != 0) {
              batch = group * 64 + static_cast<std::size_t>(std::countr_zero(bits));
              found = true;
              break;
            }
          }
          if (!found) break;
          position = first_in(batch, 0);
        }
        std::uint64_t* row = live.data() + batch * words;
        row[position / 64] &= ~(std::uint64_t{1} << (position % 64));
        if (std::all_of(row, row + words, [](std::uint64_t word) { return word == 0; }))
          live_any[batch / 64] &= ~(std::uint64_t{1} << (batch % 64));
        head_batch = batch;
        head_position = static_cast<std::ptrdiff_t>(position);
      }
      dirty[position / 64] &= ~(std::uint64_t{1} << (position % 64));
      const auto id = batch_marking[last_holding(position)];
      const auto pair = pair_of[position];
      auto& word = visited[std::size_t(id) * pair_words + pair / 64];
      const std::uint64_t bit = std::uint64_t{1} << (pair % 64);
      if (word & bit) continue;
      word |= bit;
      if (!silent_at[position]) { selected.push_back(order[position]); continue; }
      if (budget && expanded >= *budget) throw detail::SilentSearchBudgetExhausted();
      if (expanded++ >= kSilentSearchCap && !budget)
        throw std::runtime_error("enabled transition diagnostic search exceeded limit");
      if (rows >= kEmpty - 1) return FlatSearch::kDeclined;
      reserve(pool, (rows + 1) * places);
      Cell* row = pool.data() + rows * places;
      const Cell* parent = pool.data() + std::size_t(id) * places;
      std::copy_n(parent, places, row);
      if (!flat_fire(*flat[position], row)) return kOutOfRange;
      std::uint64_t h = hashes[id];
      for (const auto place : touched_places[position])
        h += mix(place, row[place]) - mix(place, parent[place]);
      const auto [reached, added] = intern_next(h);
      if (added) {
        // Only transitions reading a touched place can change enabledness.
        std::uint64_t* enabled = enabled_rows.data() + std::size_t(reached) * words;
        const std::uint64_t* inherited = enabled_rows.data() + std::size_t(id) * words;
        for (std::size_t word = 0; word < words; ++word) enabled[word] = inherited[word];
        std::size_t count = enabled_count[id];
        for (const auto place : touched_places[position]) {
          for (const auto other : readers[place]) {
            const std::uint64_t mask = std::uint64_t{1} << (other % 64);
            const bool was = (enabled[other / 64] & mask) != 0;
            if (flat_enabled(*flat[other], row) == was) continue;
            enabled[other / 64] ^= mask;
            if (was) --count; else ++count;
          }
        }
        enabled_count[reached] = static_cast<std::uint32_t>(count);
      }
      discover(reached);
    }
    return FlatSearch::kDone;
  }

  // What a flat search found: a path, proof of no path within the cap, or
  // that it declined and silent_path must decide.
  enum class FlatOutcome { kFound, kNone, kDeclined };

  // `targets` non-null: the goal is any of them enabled; null: the goal is
  // every final place marked (marking_has_final_places).
  [[nodiscard]] FlatOutcome flat_silent_path(const Marking& start,
                                             const std::vector<std::size_t>* targets,
                                             std::vector<std::size_t>& path) const {
    const std::size_t places = start.size();
    std::vector<const FlatTransition*> silent;
    silent.reserve(silent_transitions_.size());
    for (const std::size_t s : silent_transitions_) {
      const auto& flat = flat_transition(s, places);
      if (!flat.usable) return FlatOutcome::kDeclined;
      silent.push_back(&flat);
    }
    std::vector<const FlatTransition*> goal_transitions;
    std::vector<std::uint32_t> final_places;
    if (targets) {
      for (const std::size_t t : *targets) {
        const auto& flat = flat_transition(t, places);
        if (!flat.usable) return FlatOutcome::kDeclined;
        goal_transitions.push_back(&flat);
      }
    } else {
      if (final_.size() < places) return FlatOutcome::kDeclined;
      for (std::size_t place = 0; place < places; ++place)
        if (final_[place] > 0) final_places.push_back(static_cast<std::uint32_t>(place));
      // marking_has_final_places reads final_ over the marking's places only.
    }
    const auto goal = [&](const std::int32_t* row) {
      if (targets) {
        for (const auto* t : goal_transitions)
          if (flat_enabled(*t, row)) return true;
        return false;
      }
      for (const auto place : final_places)
        if (row[place] <= 0) return false;
      return true;
    };

    // The search's buffers are kept between calls (a capped search can
    // discover tens of thousands of rows), and row hashes are additive over
    // places, so a successor's hash follows from its parent's and the places
    // the firing touched. The hash only buckets rows (equality is checked in
    // full), so the rows visited and the path found are unchanged.
    auto& scratch = flat_search_scratch_;
    auto& pool = scratch.pool;
    pool.resize(places);
    for (std::size_t place = 0; place < places; ++place) {
      const auto& value = start[place];
      if (!value.is_small() || value.small_value() < std::numeric_limits<std::int32_t>::min() ||
          value.small_value() > std::numeric_limits<std::int32_t>::max())
        return FlatOutcome::kDeclined;
      pool[place] = static_cast<std::int32_t>(value.small_value());
    }
    if (goal(pool.data())) { path.clear(); return FlatOutcome::kFound; }

    const auto mix = [](std::size_t place, std::int32_t value) {
      std::uint64_t x = (std::uint64_t(place) << 32) ^ static_cast<std::uint32_t>(value);
      x += 0x9e3779b97f4a7c15ull;
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
      return x ^ (x >> 31);
    };
    constexpr std::uint32_t kEmpty = std::numeric_limits<std::uint32_t>::max();
    auto& table = scratch.table;
    auto& hashes = scratch.hashes;
    auto& parent = scratch.parent;
    auto& via = scratch.via;
    auto& frontier = scratch.frontier;
    auto& row_buffer = scratch.row;
    table.assign(1024, kEmpty);
    hashes.clear();
    parent.clear();
    via.clear();
    frontier.clear();
    row_buffer.resize(places);
    std::size_t used = 0;
    // Adds `row` (a pool row or the row buffer) as row `hashes.size()` unless
    // an equal row is known; true when it was new.
    const auto insert = [&](const std::int32_t* row, std::uint64_t h) {
      if ((used + 1) * 2 > table.size()) {
        auto& grown = scratch.grown;
        grown.assign(table.size() * 2, kEmpty);
        const std::size_t mask = grown.size() - 1;
        for (const auto existing : table) {
          if (existing == kEmpty) continue;
          std::size_t slot = hashes[existing] & mask;
          while (grown[slot] != kEmpty) slot = (slot + 1) & mask;
          grown[slot] = existing;
        }
        table.swap(grown);
      }
      const std::size_t mask = table.size() - 1;
      std::size_t slot = h & mask;
      while (table[slot] != kEmpty) {
        const auto other = table[slot];
        if (hashes[other] == h &&
            std::equal(row, row + places, pool.data() + std::size_t(other) * places))
          return false;
        slot = (slot + 1) & mask;
      }
      table[slot] = static_cast<std::uint32_t>(hashes.size());
      hashes.push_back(h);
      ++used;
      return true;
    };
    {
      std::uint64_t h = 0;
      for (std::size_t place = 0; place < places; ++place) h += mix(place, pool[place]);
      insert(pool.data(), h);
    }
    parent.push_back(kEmpty);
    via.push_back(kEmpty);

    // Only the silent transitions a marked place (or no place) can enable
    // are checked, in index order: each other one has a positive requirement
    // on an empty place, so flat_enabled is false for it.
    auto& index_entry = flat_silent_index_;
    if (index_entry.places != places) {
      index_entry.places = places;
      index_entry.triggered.assign(places, {});
      index_entry.unconditional.clear();
      index_entry.touched.assign(silent.size(), {});
      for (std::size_t k = 0; k < silent.size(); ++k) {
        const auto& transition = *silent[k];
        auto& touched = index_entry.touched[k];
        for (const auto& input : transition.inputs) touched.push_back(input.first);
        for (const auto place : transition.resets) touched.push_back(place);
        for (const auto& output : transition.outputs) touched.push_back(output.first);
        std::sort(touched.begin(), touched.end());
        touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
        if (transition.never) continue;
        const auto trigger = std::find_if(transition.checks.begin(), transition.checks.end(),
                                          [](const auto& check) { return check.second > 0; });
        if (trigger == transition.checks.end()) index_entry.unconditional.push_back(static_cast<std::uint32_t>(k));
        else index_entry.triggered[trigger->first].push_back(static_cast<std::uint32_t>(k));
      }
    }
    auto& candidates = scratch.candidates;
    candidates.assign((silent.size() + 63) / 64, 0);
    auto& enabled_silent = scratch.enabled;

    frontier.push_back(0);
    std::size_t head = 0;
    std::size_t expanded = 0;
    while (head < frontier.size() && expanded++ < kSilentSearchCap) {
      const std::uint32_t index = frontier[head++];
      {
        const std::int32_t* row = pool.data() + std::size_t(index) * places;
        std::fill(candidates.begin(), candidates.end(), 0);
        for (const auto k : index_entry.unconditional) candidates[k >> 6] |= std::uint64_t{1} << (k & 63);
        for (std::size_t place = 0; place < places; ++place) {
          if (row[place] <= 0) continue;
          for (const auto k : index_entry.triggered[place]) candidates[k >> 6] |= std::uint64_t{1} << (k & 63);
        }
        enabled_silent.clear();
        for (std::size_t word = 0; word < candidates.size(); ++word)
          for (auto bits = candidates[word]; bits != 0; bits &= bits - 1)
            enabled_silent.push_back(static_cast<std::uint32_t>(word * 64 + std::countr_zero(bits)));
      }
      // Each successor is fired in place on a copy of the expanded row and
      // undone afterwards (a firing changes only its touched places). The
      // goal is tested before the duplicate check: a known row already
      // failed it when it was found, so only a new row can meet it, as when
      // the test followed the check. A row found once the frontier holds
      // kSilentSearchCap rows would sit past the last row the loop can
      // expand, so beyond its goal test it changes nothing and is not kept.
      std::copy_n(pool.data() + std::size_t(index) * places, places, row_buffer.data());
      for (const std::size_t k : enabled_silent) {
        const auto& transition = *silent[k];
        if (!flat_enabled(transition, row_buffer.data())) continue;
        if (hashes.size() >= kEmpty) return FlatOutcome::kDeclined;
        if (!flat_fire(transition, row_buffer.data())) return FlatOutcome::kDeclined;
        if (goal(row_buffer.data())) {
          path.clear();
          path.push_back(silent_transitions_[k]);
          for (std::uint32_t i = index; i != 0; i = parent[i])
            path.push_back(silent_transitions_[via[i]]);
          std::reverse(path.begin(), path.end());
          return FlatOutcome::kFound;
        }
        const auto& touched = index_entry.touched[k];
        if (frontier.size() < kSilentSearchCap) {
          const std::int32_t* from = pool.data() + std::size_t(index) * places;
          std::uint64_t h = hashes[index];
          for (const auto place : touched)
            h += mix(place, row_buffer[place]) - mix(place, from[place]);
          if (insert(row_buffer.data(), h)) {
            const auto id = static_cast<std::uint32_t>(hashes.size() - 1);
            pool.insert(pool.end(), row_buffer.begin(), row_buffer.end());
            parent.push_back(index);
            via.push_back(static_cast<std::uint32_t>(k));
            frontier.push_back(id);
          }
        }
        const std::int32_t* from = pool.data() + std::size_t(index) * places;
        for (const auto place : touched) row_buffer[place] = from[place];
      }
    }
    // Rows left unexpanded, or dropped once the frontier was full, mean the
    // search stopped at its bound rather than exhausting the silent closure.
    if (head < frontier.size() || frontier.size() >= kSilentSearchCap) ++capped_searches_;
    return FlatOutcome::kNone;
  }

  // silent_path with an any-of-`targets`-enabled goal.
  [[nodiscard]] std::optional<std::vector<std::size_t>> silent_path_to_enable(
      const Marking& start, const std::vector<std::size_t>& targets) const {
    std::vector<std::size_t> path;
    switch (flat_silent_path(start, &targets, path)) {
      case FlatOutcome::kFound: return path;
      case FlatOutcome::kNone: return std::nullopt;
      case FlatOutcome::kDeclined: break;
    }
    return silent_path(start, [&](const Marking& m) { return any_enabled(m, targets); });
  }

  // silent_path with the every-final-place-marked goal.
  [[nodiscard]] std::optional<std::vector<std::size_t>> silent_path_to_final(
      const Marking& start) const {
    std::vector<std::size_t> path;
    switch (flat_silent_path(start, nullptr, path)) {
      case FlatOutcome::kFound: return path;
      case FlatOutcome::kNone: return std::nullopt;
      case FlatOutcome::kDeclined: break;
    }
    return silent_path(start, [&](const Marking& candidate) {
      return marking_has_final_places(candidate, final_);
    });
  }

  // Breadth-first search over silent-transition firings for a marking meeting
  // `goal`. Returns the silent transitions to fire, or std::nullopt if `goal`
  // is unreachable without creating tokens. Visited states live in an arena
  // with parent links, so the found path is reconstructed once at the end
  // instead of being copied into every frontier entry.
  template <typename Goal>
  [[nodiscard]] std::optional<std::vector<std::size_t>> silent_path(
      const Marking& start, const Goal& goal) const {
    if (goal(start)) return std::vector<std::size_t>{};

    constexpr std::size_t kStart = std::numeric_limits<std::size_t>::max();
    struct Visited {
      Marking marking;
      std::size_t transition;
      std::size_t parent;
    };
    std::vector<Visited> arena;
    // The visited set holds arena indices, so every reached marking is
    // stored once (a capped search can reach hundreds of thousands).
    const auto marking_at = [&](std::size_t i) -> const Marking& {
      return i == kStart ? start : arena[i].marking;
    };
    const auto hash = [&](std::size_t i) { return MarkingHash{}(marking_at(i)); };
    const auto equal = [&](std::size_t a, std::size_t b) { return marking_at(a) == marking_at(b); };
    std::unordered_set<std::size_t, decltype(hash), decltype(equal)> seen(16, hash, equal);
    seen.insert(kStart);
    std::queue<std::size_t> frontier;
    frontier.push(kStart);
    std::size_t expanded = 0;

    const auto reconstruct = [&](std::size_t index) {
      std::vector<std::size_t> path;
      for (std::size_t i = index; i != kStart; i = arena[i].parent) {
        path.push_back(arena[i].transition);
      }
      std::reverse(path.begin(), path.end());
      return path;
    };

    while (!frontier.empty() && expanded++ < kSilentSearchCap) {
      const std::size_t index = frontier.front();
      frontier.pop();
      // Copy: growing the arena below would invalidate a reference.
      const Marking current = marking_at(index);
      for (const std::size_t s : silent_transitions_) {
        if (!enabled(current, s)) continue;
        Marking next = current;
        fire_marking(next, s);
        arena.push_back(Visited{std::move(next), s, index});
        if (!seen.insert(arena.size() - 1).second) {
          arena.pop_back();
          continue;
        }
        if (goal(arena.back().marking)) return reconstruct(arena.size() - 1);
        frontier.push(arena.size() - 1);
      }
    }
    if (!frontier.empty()) ++capped_searches_;  // stopped at the bound
    return std::nullopt;
  }

  // Reverse silent routing used by the deprecated backwards variation. The
  // search state is the marking required immediately before the visible
  // transition. A silent transition can precede it when its output multiset
  // covers that requirement; inverse application removes available output
  // tokens and adds the transition's inputs. A global visited-transition set,
  // marking deduplication, and the expansion cap keep this bounded/deterministic.
  [[nodiscard]] std::optional<std::vector<std::size_t>> reverse_silent_path(
      const Marking& marking, std::size_t visible) const {
    const auto& target = net_.transitions()[visible];
    if (target.has_extended_arcs()) return std::nullopt;
    Marking required(marking.size(), 0);
    for (std::size_t index = 0; index < target.inputs.size(); ++index)
      required[target.inputs[index]] += arc_tokens(target.input_arc_weight(index));
    const auto covers = [](const Marking& have, const Marking& need) {
      for (std::size_t p = 0; p < have.size(); ++p) {
        if (have[p] < need[p]) return false;
      }
      return true;
    };
    if (covers(marking, required)) return std::vector<std::size_t>{};

    constexpr std::size_t kStart = std::numeric_limits<std::size_t>::max();
    struct Visited {
      Marking required;
      std::size_t transition;
      std::size_t parent;
    };
    std::vector<Visited> arena;
    std::unordered_set<Marking, MarkingHash> seen{required};
    std::queue<std::size_t> frontier;
    frontier.push(kStart);
    std::vector<char> expanded(net_.transitions().size(), 0);
    const auto reconstruct = [&](std::size_t index) {
      std::vector<std::size_t> path;
      for (std::size_t i = index; i != kStart; i = arena[i].parent) {
        path.push_back(arena[i].transition);
      }
      // Parent links lead from the discovered predecessor toward the visible
      // requirement, which is already forward firing order here.
      return path;
    };

    // Each silent transition's produced tokens per place, built on first use
    // (in the same order, so an invalid weight throws where it did).
    std::vector<std::optional<Marking>> output_counts(net_.transitions().size());
    // The global transition-visit rule bounds the search by the number of
    // silent transitions, so the legacy variation needs no heuristic cap.
    while (!frontier.empty()) {
      const std::size_t index = frontier.front();
      frontier.pop();
      const Marking current = index == kStart ? required : arena[index].required;
      for (const std::size_t s : silent_transitions_) {
        if (expanded[s]) continue;
        const auto& transition = net_.transitions()[s];
        if (transition.has_extended_arcs()) continue;
        bool output_covers = true;
        if (!output_counts[s]) {
          Marking built(marking.size(), 0);
          for (std::size_t index = 0; index < transition.outputs.size(); ++index)
            built[transition.outputs[index]] += arc_tokens(transition.output_arc_weight(index));
          output_counts[s] = std::move(built);
        }
        const Marking& output_count = *output_counts[s];
        for (std::size_t p = 0; p < current.size(); ++p) {
          if (output_count[p] < current[p]) {
            output_covers = false;
            break;
          }
        }
        if (!output_covers) continue;
        expanded[s] = 1;

        Marking predecessor = current;
        // Subtract an output bundle only when the requirement carries its full
        // arc weight. Repeated place ids are Minera's representation of a
        // weighted arc and therefore must be handled as one bundle.
        for (std::size_t p = 0; p < predecessor.size(); ++p) {
          if (output_count[p] != 0 &&
              predecessor[p] >= output_count[p]) {
            predecessor[p] -= output_count[p];
          }
        }
        for (std::size_t index = 0; index < transition.inputs.size(); ++index)
          predecessor[transition.inputs[index]] += arc_tokens(transition.input_arc_weight(index));
        if (!seen.insert(predecessor).second) continue;
        arena.push_back(Visited{predecessor, s, index});
        const std::size_t found = arena.size() - 1;
        if (covers(marking, arena[found].required)) {
          auto path = reconstruct(found);
          return path;
        }
        frontier.push(found);
      }
    }
    return std::nullopt;
  }

  // Fires silent transitions (if a no-cost path exists) so that one of
  // `targets` becomes enabled, updating the marking and counts in place. The
  // firings along the chosen path are appended to `fired` when given.
  void route_silently(Marking& marking,
                      const std::vector<std::size_t>& targets,
                      Counts& counts,
                      std::vector<std::size_t>* fired = nullptr) const {
    if (any_enabled(marking, targets)) return;
    // A search that cannot succeed would still run to its cap; the relaxed
    // fixpoint disproves most of those misses for the price of one pass.
    const auto reachable = relaxed_silent_places(marking);
    if (std::none_of(targets.begin(), targets.end(), [&](std::size_t t) {
          return relaxed_enables(reachable, t);
        })) {
      return;  // provably no silent route; the visible firing creates tokens
    }
    const auto path = silent_path_to_enable(marking, targets);
    if (!path) return;  // no silent route; the visible firing will create tokens
    for (const std::size_t s : *path) fire(marking, s, counts);
    if (fired) fired->insert(fired->end(), path->begin(), path->end());
  }

  void route_silently_to_marking(
      Marking& marking, Counts& counts,
      std::vector<std::size_t>* fired = nullptr) const {
    if (marking_has_final_places(marking, final_)) return;
    // A final place that no silent firing can ever mark rules the route out
    // (the relaxed fixpoint over-approximates the silently markable places),
    // so skip the search, which could only end at its cap without a path.
    const auto reachable = relaxed_silent_places(marking);
    for (std::size_t place = 0; place < final_.size() && place < reachable.size(); ++place)
      if (final_[place] > 0 && !reachable[place]) return;
    const auto path = silent_path_to_final(marking);
    if (!path) return;
    for (const std::size_t s : *path) fire(marking, s, counts);
    if (fired) fired->insert(fired->end(), path->begin(), path->end());
  }

  std::optional<PetriNet> ordinary_net_;
  const PetriNet& net_;
  ResolvedReplayMarkings markings_;
  Marking initial_;
  Marking final_;
  bool full_result_ = false;
  std::optional<ForwardConfiguration> forward_configuration_;
  std::unordered_map<std::string, std::size_t> name_to_activity_;
  std::vector<std::vector<std::size_t>> transitions_by_activity_;
  std::vector<std::size_t> silent_transitions_;
  mutable std::unordered_map<std::pair<Marking, std::size_t>, Step, StepKeyHash>
      step_cache_, direct_step_cache_;
  mutable std::unordered_map<Marking, Step, MarkingHash> finish_cache_;
  // Silent-routing searches (silent_path, flat_silent_path) that stopped at
  // kSilentSearchCap; cached steps record whether theirs did.
  mutable std::size_t capped_searches_ = 0;
  mutable std::optional<detail::SilentPlacePathIndex> place_path_index_;
  mutable std::vector<std::optional<ArcWeights>> arc_weights_;
  mutable std::vector<std::optional<InputChecks>> input_checks_;
  mutable std::vector<std::optional<FireInputs>> fire_inputs_;
  bool extended_arcs_ = false;  // net_.has_extended_arcs(), which scans every transition
  bool place_closure_exact_ = false;
  std::vector<std::vector<std::size_t>> place_enables_;
  mutable std::vector<FlatTransition> flat_transitions_;
  mutable std::size_t flat_places_ = std::numeric_limits<std::size_t>::max();
  // flat_silent_path's candidate index over silent_transitions_ (positions),
  // by the first place each one needs a positive count on.
  struct FlatSilentIndex {
    std::size_t places = std::numeric_limits<std::size_t>::max();
    std::vector<std::vector<std::uint32_t>> triggered;
    std::vector<std::uint32_t> unconditional;
    std::vector<std::vector<std::uint32_t>> touched;  // places each firing may change
  };
  mutable FlatSilentIndex flat_silent_index_;
  // flat_silent_path's buffers, reused so a search allocates nothing new.
  struct FlatSearchScratch {
    std::vector<std::int32_t> pool, row;
    std::vector<std::uint64_t> hashes, candidates;
    std::vector<std::uint32_t> table, grown, parent, via, frontier, enabled;
  };
  mutable FlatSearchScratch flat_search_scratch_;
  // flat_forward_enabled's buffers, reused across the searches of one walk.
  struct FlatEnabledScratch {
    std::vector<const FlatTransition*> by_transition, flat;
    std::vector<std::int32_t> pool;
    std::vector<std::int8_t> small_pool;
    std::vector<std::uint64_t> hashes, visited, enabled, batch_rows, live, live_any, dirty, columns;
    std::vector<std::size_t> batch_start, block_column;
    std::vector<std::uint32_t> batch_marking, enabled_count, table, grown;
    std::vector<std::size_t> table_used;  // slots of `table` holding a row id
  };
  mutable FlatEnabledScratch flat_enabled_scratch_;
};

// Collapses the log into distinct activity-id sequences with occurrence counts,
// preserving first-seen order so results are deterministic.
struct Variant {
  std::vector<std::optional<std::size_t>> ids;  // std::nullopt = absent activity
  std::vector<std::string> names;
  // Per step: the plain string label (std::nullopt for null or typed
  // activities) and the step's identity key, as token precision groups them.
  std::vector<std::optional<std::string>> labels;
  std::vector<std::string> identities;
  std::uint64_t count = 0;
  std::vector<std::size_t> null_activity_indices;
};

struct ReplayActivity {
  // display_name is absent only for an explicit null.  A present empty string
  // remains distinguishable from null, even when it has no model transition.
  std::optional<std::string> display_name;
  // Only a scalar string attribute can identify a transition label.
  std::optional<std::string> model_label;
  std::string variant_key;
};

void append_identity_field(std::string& key, char tag, std::string_view value) {
  key.push_back(tag);
  key += std::to_string(value.size());
  key.push_back(':');
  key.append(value);
}

template <typename Number>
void append_identity_number(std::string& key, char tag, Number value) {
  append_identity_field(key, tag, std::to_string(value));
}

void append_native_value_identity(std::string& key,
                                  const NativeAttributeValue& value) {
  const auto& native = static_cast<const NativeAttributeValue::Base&>(value);
  append_identity_number(key, 'v', native.index());
  std::visit(
      [&](const auto& scalar) {
        using Scalar = std::remove_cvref_t<decltype(scalar)>;
        if constexpr (std::is_same_v<Scalar, std::int64_t> ||
                      std::is_same_v<Scalar, std::uint64_t>) {
          append_identity_number(key, 'n', scalar);
        } else if constexpr (std::is_same_v<Scalar, double>) {
          append_identity_number(key, 'n', std::bit_cast<std::uint64_t>(scalar));
        } else if constexpr (std::is_same_v<Scalar, bool>) {
          append_identity_number(key, 'n', scalar ? 1 : 0);
        } else if constexpr (std::is_same_v<Scalar, NativeDate>) {
          append_identity_field(key, 'd', scalar.value);
          append_identity_number(key, 'r',
                                static_cast<int>(scalar.resolution));
          append_identity_number(key, 'z', scalar.timezone.has_value() ? 1 : 0);
          if (scalar.timezone)
            append_identity_field(key, 't', *scalar.timezone);
        }
      },
      native);
}

void append_attribute_identity(std::string& key,
                               const EventAttribute& attribute) {
  key.push_back('A');
  append_identity_number(key, 'k', static_cast<int>(attribute.kind));
  append_identity_number(key, 'c', static_cast<int>(attribute.container));
  append_identity_number(key, 'w', attribute.scalar_wrapper ? 1 : 0);
  append_identity_number(key, 'l', attribute.scalar_children_list ? 1 : 0);
  append_identity_number(
      key, '0', attribute.is_null ||
                   attribute.first_class_state == FirstClassAttributeState::kNull);
  append_identity_field(key, 's', event_attribute_lexical_value(attribute));
  append_native_value_identity(key, attribute.typed_value);
  append_identity_number(key, 'q', attribute.children.size());
  for (const auto& child : attribute.children) {
    append_identity_field(key, 'e', child.key);
    append_attribute_identity(key, child);
  }
}

ReplayActivity replay_activity_from_attribute(
    const EventAttribute& attribute, std::string_view selector = "concept:name") {
  if (attribute.first_class_role != FirstClassAttributeRole::kNone &&
      attribute.first_class_state == FirstClassAttributeState::kMissing)
    throw TokenReplayKeyError(std::string(selector));
  if (attribute.is_null ||
      attribute.first_class_state == FirstClassAttributeState::kNull) {
    return {std::nullopt, std::nullopt, "null"};
  }

  ReplayActivity result;
  result.display_name = std::string(event_attribute_lexical_value(attribute));
  if (attribute.kind == AttributeKind::kString &&
      attribute.container == AttributeContainerKind::kScalar &&
      std::holds_alternative<std::monostate>(
          static_cast<const NativeAttributeValue::Base&>(
              attribute.typed_value))) {
    // Wrapper/list metadata belongs to the selected scalar's retained XES
    // detail. It does not change the scalar string that names an activity.
    result.model_label = *result.display_name;
    result.variant_key = "string";
    append_identity_field(result.variant_key, 's', *result.display_name);
    return result;
  }
  append_attribute_identity(result.variant_key, attribute);
  return result;
}

ReplayActivity replay_activity_from_legacy_string(std::string_view activity) {
  EventAttribute attribute;
  attribute.value = std::string(activity);
  return replay_activity_from_attribute(attribute);
}

std::string selected_activity(const Event& event,
                              std::string_view activity_attribute) {
  return reference_selected_activity(event, activity_attribute, "conformance");
}

ReplayActivity selected_replay_activity(
    const Event& event, std::string_view selector) {
  const EventAttribute* selected = nullptr;
  int priority = 0;
  std::size_t matches = 0;
  for (const auto& attribute : event.attributes) {
    const int candidate = attribute.key == selector ? 3
        : !attribute.first_class_source_key.empty() &&
          attribute.first_class_source_key == selector ? 2 : 0;
    if (candidate == 0 || candidate < priority) continue;
    if (candidate > priority) {
      selected = &attribute;
      priority = candidate;
      matches = 1;
    } else {
      ++matches;
    }
  }
  if (matches > 1)
    throw std::invalid_argument("conformance: duplicate selected activity attribute");
  if (selected) {
    return replay_activity_from_attribute(*selected, selector);
  }
  // A first-class projection represents its source key. Only legacy text
  // without source metadata represents the default concept:name key.
  const std::string_view projected_key = !event.activity_source_key.empty()
      ? std::string_view(event.activity_source_key)
      : event.activity_attribute
          ? !event.activity_attribute->first_class_source_key.empty()
              ? std::string_view(event.activity_attribute->first_class_source_key)
              : std::string_view(event.activity_attribute->key)
          : std::string_view("concept:name");
  const bool selected_first_class_activity = selector == projected_key;
  if (selected_first_class_activity) {
    if (event.activity_state == FirstClassAttributeState::kNull)
      return {std::nullopt, std::nullopt, "null"};
    if (event.activity_attribute)
      return replay_activity_from_attribute(*event.activity_attribute, selector);
    if (event.has_activity())
      return replay_activity_from_legacy_string(event.activity);
  }
  throw TokenReplayKeyError(std::string(selector));
}

std::vector<Variant> collect_replay_variants(
    const EventLog& log, const Replayer& resolver, std::string_view selector) {
  std::vector<Variant> variants;
  std::map<std::vector<std::string>, std::size_t> index;
  for (const auto& trace : log.traces) {
    std::vector<std::string> key;
    Variant variant;
    for (const auto& event : trace.events) {
      auto activity = selected_replay_activity(event, selector);
      if (!activity.display_name)
        variant.null_activity_indices.push_back(variant.names.size());
      variant.ids.push_back(activity.model_label
                                ? resolver.activity_id(*activity.model_label)
                                : std::nullopt);
      variant.names.push_back(activity.display_name.value_or(""));
      variant.labels.push_back(activity.model_label);
      variant.identities.push_back(activity.variant_key);
      key.push_back(std::move(activity.variant_key));
    }
    const auto [it, inserted] = index.emplace(std::move(key), variants.size());
    if (inserted) {
      variant.count = 1;
      variants.push_back(std::move(variant));
    } else {
      ++variants[it->second].count;
    }
  }
  return variants;
}

// `resolver.activity_id(name)` maps an activity name to its model id (or
// std::nullopt when absent), so this works for any net wrapper.
template <typename Resolver>
std::vector<Variant> collect_variants(const EventLog& log,
                                      const Resolver& resolver,
                                      std::string_view activity_attribute =
                                          "concept:name",
                                      const std::vector<std::size_t>* trace_order = nullptr) {
  if (trace_order) {
    std::vector<bool> seen(log.traces.size(), false);
    if (trace_order->size() != log.traces.size())
      throw std::invalid_argument("alignment trace order must list every trace once");
    for (const auto position : *trace_order) {
      if (position >= seen.size() || seen[position])
        throw std::invalid_argument("alignment trace order must list every trace once");
      seen[position] = true;
    }
  }
  std::vector<Variant> variants;
  std::unordered_map<std::string, std::size_t> index;
  for (std::size_t k = 0; k < log.traces.size(); ++k) {
    const auto& trace = log.traces[trace_order ? (*trace_order)[k] : k];
    std::string key;
    Variant variant;
    for (const auto& event : trace.events) {
      const auto activity = selected_activity(event, activity_attribute);
      variant.ids.push_back(resolver.activity_id(activity));
      variant.names.push_back(activity);
      key.push_back('\x1f');
      key += activity;
    }
    auto [it, inserted] = index.emplace(std::move(key), variants.size());
    if (inserted) {
      variant.count = 1;
      variants.push_back(std::move(variant));
    } else {
      ++variants[it->second].count;
    }
  }
  return variants;
}

// Alignment-based escaping-edges precision (Adriansyah, Munoz-Gama, Carmona,
// van Dongen & van der Aalst): a prefix-automaton walk like token
// ETConformance precision, but each prefix's model state comes from the optimal
// alignments rather than from forced token replay — on a deviating log the
// replay marking can be an artifact of healed tokens, while the aligned
// marking is one the model genuinely reaches in the cheapest explanation.
// Variants sharing a prefix may align it to different markings; the enabled
// sets of all of them are unioned, mirroring that any of those explanations
// is optimal.
double compute_alignment_precision(
    const std::vector<Variant>& variants,
    const std::vector<std::vector<Marking>>& variant_markings,
    const Replayer& replayer) {
  constexpr std::size_t kAbsent = std::numeric_limits<std::size_t>::max();
  struct Node {
    std::unordered_map<std::size_t, std::size_t> children;  // edge -> node
    std::uint64_t freq = 0;
    std::unordered_set<std::size_t> observed;  // next activities the log takes
    std::unordered_set<Marking, MarkingHash> markings;  // aligned model states
  };
  std::vector<Node> nodes(1);
  for (std::size_t v = 0; v < variants.size(); ++v) {
    const auto& variant = variants[v];
    const auto& markings = variant_markings[v];
    std::size_t current = 0;
    nodes[current].freq += variant.count;
    nodes[current].markings.insert(markings[0]);
    for (std::size_t d = 0; d < variant.ids.size(); ++d) {
      const auto& id = variant.ids[d];
      if (id) nodes[current].observed.insert(*id);
      const auto [it, inserted] =
          nodes[current].children.emplace(id ? *id : kAbsent, nodes.size());
      if (inserted) nodes.emplace_back();
      current = it->second;
      if (d + 1 < variant.ids.size()) {
        nodes[current].freq += variant.count;
        nodes[current].markings.insert(markings[d + 1]);
      }
    }
  }

  // Distinct prefixes routinely align to the same marking; cache the costly
  // enabled-activities closure per marking.
  std::unordered_map<Marking, std::unordered_set<std::size_t>, MarkingHash>
      enabled_cache;

  std::uint64_t enabled_total = 0;
  std::uint64_t escaping_total = 0;
  for (const auto& node : nodes) {
    std::unordered_set<std::size_t> enabled;
    for (const auto& marking : node.markings) {
      const auto [it, inserted] = enabled_cache.try_emplace(marking);
      if (inserted) it->second = replayer.enabled_activities(marking);
      enabled.insert(it->second.begin(), it->second.end());
    }
    std::uint64_t escaping = 0;
    for (const std::size_t a : enabled) {
      if (node.observed.find(a) == node.observed.end()) ++escaping;
    }
    enabled_total += node.freq * enabled.size();
    escaping_total += node.freq * escaping;
  }
  return enabled_total == 0 ? 1.0
                            : 1.0 - double(escaping_total) / double(enabled_total);
}

// Token-replay generalization (Buijs, van Dongen & van der Aalst): a model
// generalizes well when replaying the log spreads executions across its
// transitions instead of leaving parts of the net exercised only once or
// twice. Each executed transition contributes 1/sqrt(executions) — near 1 when
// barely used, vanishing as use grows — averaged over all transitions.
double compute_generalization(const std::vector<std::uint64_t>& executions) {
  if (executions.empty()) return 1.0;
  double rarity = 0.0;
  for (const auto count : executions) {
    // The evaluator penalises transitions which were never exercised as strongly as
    // a transition seen once.  Silent transitions are included by the caller
    // (their firings are part of the replay), so this remains a model-level
    // measure rather than an activity-only shortcut.
    rarity += count == 0 ? 1.0 : 1.0 / std::sqrt(double(count));
  }
  return 1.0 - rarity / double(executions.size());
}

// Inverse arc-degree simplicity: a workflow net needs two arcs per node just
// to chain behaviour, so only the mean degree beyond that counts against it.
// 1 for a plain sequence, decaying as routing density grows.
double compute_simplicity(const PetriNet& net) {
  const std::size_t nodes = net.place_count() + net.transitions().size();
  if (nodes == 0) return 1.0;
  std::size_t arcs = 0;
  for (const auto& transition : net.transitions()) {
    // Inputs/outputs may contain repeated place ids to encode a weighted arc;
    // arc-degree simplicity counts distinct arc objects, not multiplicity.
    std::unordered_set<std::size_t> in(transition.inputs.begin(), transition.inputs.end());
    std::unordered_set<std::size_t> out(transition.outputs.begin(), transition.outputs.end());
    in.insert(transition.reset_inputs.begin(), transition.reset_inputs.end());
    in.insert(transition.inhibitor_inputs.begin(), transition.inhibitor_inputs.end());
    arcs += in.size() + out.size();
  }
  // Every arc joins one place and one transition, so it adds 2 to the total
  // degree.
  const double mean_degree = 2.0 * double(arcs) / double(nodes);
  return 1.0 / (1.0 + std::max(mean_degree - 2.0, 0.0));
}

// Bounds the alignment search so a pathological model can never run forever; the
// bounded, block-structured nets the inductive miner produces reach the final
// marking well within this many state expansions.
constexpr std::size_t kAlignSearchCap = 200000;

// --- Linear program for the marking-equation heuristic ----------------------

struct LpResult {
  bool feasible = false;
  bool aborted = false;  // iteration guard tripped: no usable bound, prune nothing
  double objective = 0.0;
  std::vector<double> solution;  // per variable; empty when infeasible
};

// Constraint matrix as sparse rows: row i's nonzero entries are
// [start[i], start[i + 1]) of col/val, in ascending column order.
struct LpRows {
  std::size_t columns = 0;
  std::vector<std::size_t> start{0};
  std::vector<std::size_t> col;
  std::vector<double> val;
};

template <class Matrix>
LpRows lp_rows(const Matrix& a, std::size_t rows, std::size_t columns) {
  LpRows out;
  out.columns = columns;
  out.start.reserve(rows + 1);
  for (std::size_t i = 0; i < rows; ++i) {
    for (std::size_t j = 0; j < columns; ++j) {
      const double v = a(i, j);
      if (v != 0.0) {
        out.col.push_back(j);
        out.val.push_back(v);
      }
    }
    out.start.push_back(out.col.size());
  }
  return out;
}

struct LpWorkspace {
  std::vector<double> tab, z;
  std::vector<std::size_t> basis, nonzero;
  std::vector<char> negated;
  std::vector<std::uint64_t> row_support, column_support, updated;
};

// Calls f(index) for each set bit of words[0, count), in ascending order.
template <class F>
inline void for_each_bit(const std::uint64_t* words, std::size_t count, F&& f) {
  for (std::size_t w = 0; w < count; ++w) {
    for (std::uint64_t x = words[w]; x != 0; x &= x - 1)
      f(w * 64 + static_cast<std::size_t>(std::countr_zero(x)));
  }
}

// Minimal dense two-phase primal simplex for `min c·x  s.t.  A·x = b, x >= 0`.
// Sized for alignment heuristics (tens of rows and columns) and tuned for
// robustness over speed: Bland's rule prevents cycling, and if the iteration
// guard ever trips the caller falls back to a heuristic of zero rather than
// risking an overestimate from an unfinished minimization.
// Rows whose b is negative are negated on the way into the tableau. The
// tableau is dense, but every loop that a zero entry cannot affect visits only
// the row and column supports, in the dense loop's order: skipping x - f * 0
// or x / p for x == 0 changes no nonzero value (at most a zero's sign, which
// no comparison here can see), so the pivots, basis, objective and solution
// are those of the plain dense sweep.
// With `dual`, a solved feasible program also leaves its optimal dual there
// (y with y·b = objective and c - yA >= 0 within the pivoting tolerance), read
// off the final reduced costs of the artificial columns; nothing else
// changes.
LpResult solve_lp_sparse(const LpRows& a, std::vector<double> b,
                         const std::vector<double>& c, std::vector<double>* dual = nullptr) {
  constexpr double kEps = 1e-9;
  const std::size_t m = b.size();
  const std::size_t n = c.size();
  thread_local LpWorkspace ws;

  auto& negated = ws.negated;
  negated.assign(m, 0);
  for (std::size_t i = 0; i < m; ++i) {
    if (b[i] < 0.0) {
      negated[i] = 1;
      b[i] = -b[i];
    }
  }

  const std::size_t cols = n + m;
  const std::size_t width = cols + 1;
  auto& tab = ws.tab;
  tab.assign(m * width, 0.0);
  const auto at = [&](std::size_t i, std::size_t j) -> double& { return tab[i * width + j]; };
  auto& basis = ws.basis;
  basis.resize(m);
  auto& z = ws.z;
  z.assign(cols + 1, 0.0);
  // Supports: row i's columns and column j's rows that may hold a nonzero
  // entry (a superset — eliminations may cancel). Every loop below that
  // skips zero entries visits only these, in the same ascending order.
  const std::size_t row_words = (width + 63) / 64;
  const std::size_t column_words = (m + 63) / 64;
  auto& row_support = ws.row_support;
  auto& column_support = ws.column_support;
  auto& updated = ws.updated;
  row_support.assign(m * row_words, 0);
  column_support.assign(cols * column_words, 0);
  updated.assign(column_words, 0);
  const auto mark = [&](std::size_t i, std::size_t j) {
    row_support[i * row_words + j / 64] |= std::uint64_t{1} << (j % 64);
    if (j < cols) column_support[j * column_words + i / 64] |= std::uint64_t{1} << (i % 64);
  };
  // Phase-1 reduced costs: 0 - column sums (artificials: 1 - 1). Each sum
  // adds its column's nonzero entries in ascending row order, as a dense
  // sweep would (adding a zero changes no sum).
  for (std::size_t i = 0; i < m; ++i) {
    double* const row = &tab[i * width];
    const bool flip = negated[i] != 0;
    for (std::size_t k = a.start[i]; k < a.start[i + 1]; ++k) {
      const double v = flip ? -a.val[k] : a.val[k];
      row[a.col[k]] = v;
      z[a.col[k]] += v;
      mark(i, a.col[k]);
    }
    row[n + i] = 1.0;
    mark(i, n + i);
    row[cols] = b[i];
    mark(i, cols);
    z[cols] += b[i];
    basis[i] = n + i;
  }
  for (std::size_t j = 0; j < n; ++j) z[j] = 0.0 - z[j];
  for (std::size_t j = n; j < cols; ++j) z[j] = 1.0 - 1.0;
  z[cols] = 0.0 - z[cols];

  auto& nonzero = ws.nonzero;
  nonzero.clear();
  nonzero.reserve(width);
  const auto pivot = [&](std::size_t row, std::size_t col) {
    double* const pivot_row = &tab[row * width];
    const double p = pivot_row[col];
    nonzero.clear();
    const std::uint64_t* const pivot_support = &row_support[row * row_words];
    for_each_bit(pivot_support, row_words, [&](std::size_t j) {
      if (pivot_row[j] == 0.0) return;
      pivot_row[j] /= p;
      if (pivot_row[j] != 0.0) nonzero.push_back(j);
    });
    bool any = false;
    for_each_bit(&column_support[col * column_words], column_words, [&](std::size_t i) {
      if (i == row || std::abs(at(i, col)) < kEps) return;
      double* const target = &tab[i * width];
      const double factor = target[col];
      for (const std::size_t j : nonzero) target[j] -= factor * pivot_row[j];
      std::uint64_t* const target_support = &row_support[i * row_words];
      for (std::size_t w = 0; w < row_words; ++w) target_support[w] |= pivot_support[w];
      updated[i / 64] |= std::uint64_t{1} << (i % 64);
      any = true;
    });
    if (any) {
      for (const std::size_t j : nonzero) {
        if (j == cols) continue;
        std::uint64_t* const support = &column_support[j * column_words];
        for (std::size_t w = 0; w < column_words; ++w) support[w] |= updated[w];
      }
      std::fill(updated.begin(), updated.end(), 0);
    }
    const double zf = z[col];
    if (std::abs(zf) > 0.0) {
      for (const std::size_t j : nonzero) z[j] -= zf * pivot_row[j];
    }
    basis[row] = col;
  };

  std::size_t iterations_left = 200 + 50 * (m + n);
  std::size_t degenerate = 0;
  const auto iterate = [&](std::size_t allowed_cols) -> bool {
    while (iterations_left-- > 0) {
      std::size_t enter = allowed_cols;
      if (degenerate < 24) {
        double best = -1e-7;
        for (std::size_t j = 0; j < allowed_cols; ++j) {
          if (z[j] < best) {
            best = z[j];
            enter = j;
          }
        }
      } else {
        for (std::size_t j = 0; j < allowed_cols; ++j) {
          if (z[j] < -1e-7) {
            enter = j;
            break;
          }
        }
      }
      if (enter == allowed_cols) return true;
      std::size_t leave = m;
      double best_ratio = 0.0;
      for_each_bit(&column_support[enter * column_words], column_words, [&](std::size_t i) {
        if (at(i, enter) <= kEps) return;
        const double ratio = at(i, cols) / at(i, enter);
        if (leave == m || ratio < best_ratio - kEps ||
            (ratio < best_ratio + kEps && basis[i] < basis[leave])) {
          leave = i;
          best_ratio = ratio;
        }
      });
      if (leave == m) return true;
      if (at(leave, cols) < kEps) {
        ++degenerate;
      } else {
        degenerate = 0;
      }
      pivot(leave, enter);
    }
    return false;
  };

  LpResult aborted;
  aborted.aborted = true;
  if (!iterate(cols)) return aborted;
  if (-z[cols] > 1e-6) return {};

  for (std::size_t i = 0; i < m; ++i) {
    if (basis[i] < n) continue;
    std::size_t first = n;
    for_each_bit(&row_support[i * row_words], row_words, [&](std::size_t j) {
      if (first == n && j < n && std::abs(at(i, j)) > kEps) first = j;
    });
    if (first < n) pivot(i, first);
  }

  std::fill(z.begin(), z.end(), 0.0);
  for (std::size_t j = 0; j < n; ++j) z[j] = c[j];
  for (std::size_t i = 0; i < m; ++i) {
    const double cb = basis[i] < n ? c[basis[i]] : 0.0;
    if (cb == 0.0) continue;
    const double* const row = &tab[i * width];
    for_each_bit(&row_support[i * row_words], row_words, [&](std::size_t j) {
      if (row[j] != 0.0) z[j] -= cb * row[j];
    });
  }
  if (!iterate(n)) return aborted;

  LpResult result;
  result.feasible = true;
  result.objective = -z[cols];
  result.solution.assign(n, 0.0);
  for (std::size_t i = 0; i < m; ++i) {
    if (basis[i] < n) result.solution[basis[i]] = at(i, cols);
  }
  if (dual) {
    // Artificial column n + i is e_i, so its reduced cost is -(c_B B^-1)_i,
    // the dual of tableau row i; a row negated on the way in negates it back.
    dual->assign(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) (*dual)[i] = negated[i] ? z[n + i] : -z[n + i];
  }
  return result;
}

LpResult solve_lp(const std::vector<std::vector<double>>& a,
                  std::vector<double> b,
                  const std::vector<double>& c) {
  const auto rows =
      lp_rows([&](std::size_t i, std::size_t j) { return a[i][j]; }, b.size(), c.size());
  return solve_lp_sparse(rows, std::move(b), c);
}

// Computes optimal alignments by an A* search over the synchronous product of
// the trace and the workflow net. A search state is a (marking, trace-position)
// pair; the goal is the final marking with the whole trace consumed. Edge costs
// follow the standard alignment cost function: synchronous and silent moves are
// free, log-only and visible model-only moves cost one each. The heuristic is
// the marking-equation lower bound (van Dongen), solved lazily: children
// inherit max(h - cost, 0) from their parent and the LP runs only when a state
// with an inherited bound is actually popped — so the search stays optimal
// while expanding a fraction of the states plain Dijkstra visits, which is what
// makes concurrent nets (parallel branches multiply the marking space)
// alignable within the search cap.
// Polls AlignmentOptions::cancelled every 256 calls (the callback may be a
// system call); an unset callback never cancels.
class CancelPoll {
 public:
  explicit CancelPoll(const std::function<bool()>& cancelled) : cancelled_(cancelled) {}
  bool operator()() {
    if (!cancelled_) return false;
    return (++calls_ & 255u) == 0 && cancelled_();
  }

 private:
  const std::function<bool()>& cancelled_;
  std::size_t calls_ = 0;
};

// Markings met by the model-run searches and the model-footprint
// exploration, each stored once as a row of one flat pool and numbered in
// the order they were first met. Search entries refer to markings by id;
// ties in the frontier are still broken by comparing the markings themselves
// (lexicographically, as std::vector compares), so expansion order is
// unchanged. A marking is copied into the pool only when it is new.
class MarkingTable {
 public:
  explicit MarkingTable(std::size_t places) : places_(places), slots_(1024, kEmpty) {}
  MarkingTable(const MarkingTable&) = delete;
  MarkingTable& operator=(const MarkingTable&) = delete;

  // Returns the id of the marking and whether it was added by this call.
  std::pair<std::uint32_t, bool> intern(const Marking& marking) {
    return intern(marking.data(), row_hash(marking.data()));
  }
  // Interns `count` markings stored back to back in `rows`, in order, and
  // calls visit(k, id, added) for the k-th. Their slots are prefetched first
  // (the probes of a large table miss the cache), so the ids are those of
  // interning them one at a time.
  template <class Visit>
  void intern_batch(const int* rows, std::size_t count, Visit&& visit) {
    batch_hashes_.resize(count);
    for (std::size_t k = 0; k < count; ++k) {
      batch_hashes_[k] = row_hash(rows + k * places_);
    }
    for (std::size_t k = 0; k < count; ++k) {
      const auto [id, added] = intern(rows + k * places_, batch_hashes_[k]);
      visit(k, id, added);
    }
  }
  std::pair<std::uint32_t, bool> intern(const int* row, std::uint64_t hash) {
    if ((count_ + 1) * 2 > slots_.size()) grow();
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = hash & mask;
    while (slots_[slot] != kEmpty) {
      const auto other = slots_[slot];
      if (hashes_[other] == hash && std::equal(row, row + places_, at(other)))
        return {other, false};
      slot = (slot + 1) & mask;
    }
    if (count_ >= std::numeric_limits<std::uint32_t>::max() - 1)
      throw std::length_error("model-run search marking table overflow");
    const auto id = static_cast<std::uint32_t>(count_++);
    // Fixed-size blocks: growing never copies (or transiently doubles) the
    // stored markings.
    if ((id & (kBlockRows - 1)) == 0)
      blocks_.push_back(std::make_unique_for_overwrite<int[]>(kBlockRows * places_));
    std::copy(row, row + places_, blocks_.back().get() + (id & (kBlockRows - 1)) * places_);
    hashes_.push_back(hash);
    slots_[slot] = id;
    return {id, true};
  }
  // The id of a stored marking, or std::nullopt.
  [[nodiscard]] std::optional<std::uint32_t> find(const Marking& marking) const {
    const int* row = marking.data();
    const std::uint64_t hash = row_hash(row);
    const std::size_t mask = slots_.size() - 1;
    for (std::size_t slot = hash & mask; slots_[slot] != kEmpty; slot = (slot + 1) & mask) {
      const auto other = slots_[slot];
      if (hashes_[other] == hash && std::equal(row, row + places_, at(other))) return other;
    }
    return std::nullopt;
  }
  [[nodiscard]] std::size_t size() const { return count_; }
  [[nodiscard]] const int* at(std::uint32_t id) const {
    return blocks_[id / kBlockRows].get() + std::size_t(id & (kBlockRows - 1)) * places_;
  }
  // Copies marking `id` into `out` (sized to the place count).
  void copy(std::uint32_t id, Marking& out) const {
    out.assign(at(id), at(id) + places_);
  }
  // std::vector's operator> on the two markings.
  [[nodiscard]] bool greater(std::uint32_t left, std::uint32_t right) const {
    return std::lexicographical_compare(at(right), at(right) + places_, at(left),
                                        at(left) + places_);
  }

 private:
  static constexpr std::uint32_t kEmpty = std::numeric_limits<std::uint32_t>::max();
  static constexpr std::uint32_t kBlockRows = 4096;  // a power of two
  // Only the slot layout depends on the hash (ids follow insertion order):
  // two independent multiply chains, then a final mix for the low bits the
  // mask keeps.
  [[nodiscard]] std::uint64_t row_hash(const int* row) const {
    std::uint64_t a = 0x9E3779B97F4A7C15ull, b = 0xC2B2AE3D27D4EB4Full;
    std::size_t p = 0;
    for (; p + 1 < places_; p += 2) {
      a = (a ^ static_cast<std::uint32_t>(row[p])) * 0xFF51AFD7ED558CCDull;
      b = (b ^ static_cast<std::uint32_t>(row[p + 1])) * 0xC4CEB9FE1A85EC53ull;
    }
    if (p < places_) a = (a ^ static_cast<std::uint32_t>(row[p])) * 0xFF51AFD7ED558CCDull;
    std::uint64_t h = a ^ ((b << 29) | (b >> 35));
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    return h;
  }
  void grow() {
    std::vector<std::uint32_t> grown(slots_.size() * 2, kEmpty);
    const std::size_t mask = grown.size() - 1;
    for (std::uint32_t id = 0; id < count_; ++id) {
      std::size_t slot = hashes_[id] & mask;
      while (grown[slot] != kEmpty) slot = (slot + 1) & mask;
      grown[slot] = id;
    }
    slots_.swap(grown);
  }
  std::size_t places_;
  std::size_t count_ = 0;
  std::vector<std::unique_ptr<int[]>> blocks_;
  std::vector<std::uint64_t> hashes_;
  std::vector<std::uint32_t> slots_;
  std::vector<std::uint64_t> batch_hashes_;
};

// Whether every firing changes the marking by exactly its transition's
// incidence column, so that the marking equation is a relaxation of firing
// and its LP bounds stay admissible. A reset arc clears its place, and two
// weighted input arcs on one place are each checked on their own and clamp
// the place at zero (fire_petri_transition), so either breaks it. Inhibitor
// arcs only restrict when a transition is enabled; the firing still adds its
// column, so they keep the relaxation valid.
bool marking_equation_describes_firing(const PetriNet& net) {
  for (const auto& transition : net.transitions()) {
    if (!transition.reset_inputs.empty()) return false;
    if (transition.input_weights.empty()) continue;
    for (std::size_t i = 0; i < transition.inputs.size(); ++i)
      for (std::size_t j = 0; j < i; ++j)
        if (transition.inputs[i] == transition.inputs[j]) return false;
  }
  return true;
}

class Aligner {
 public:
  explicit Aligner(
      const PetriNet& net, const AlignmentOptions& options = AlignmentOptions{},
      std::chrono::steady_clock::time_point deadline =
          std::chrono::steady_clock::time_point::max(),
      bool reachability_only = false)
      : net_(net),
        initial_(resolve_workflow_marking(net, options.initial_marking, true)),
        final_(resolve_workflow_marking(net, options.final_marking, false)) {
    for (std::size_t i = 0; i < net_.activities().size(); ++i) {
      name_to_activity_.emplace(net_.activities()[i], i);
    }
    // Net incidence (tokens produced minus consumed per place) as sparse
    // columns for the marking-equation heuristic.
    incidence_.resize(net_.transitions().size());
    for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
      std::unordered_map<std::size_t, double> column;
      const auto& transition = net_.transitions()[t];
      const auto signed_weight = [](PetriArcWeight weight) {
        const auto magnitude = static_cast<double>(weight.magnitude);
        return weight.negative ? -magnitude : magnitude;
      };
      for (std::size_t k = 0; k < transition.inputs.size(); ++k)
        column[transition.inputs[k]] -= signed_weight(transition.input_arc_weight(k));
      for (std::size_t k = 0; k < transition.outputs.size(); ++k)
        column[transition.outputs[k]] += signed_weight(transition.output_arc_weight(k));
      for (const auto& [place, coef] : column) {
        if (coef != 0.0) incidence_[t].emplace_back(place, coef);
      }
    }

    if (deadline == std::chrono::steady_clock::time_point::max()) {
      const double limit = options.max_total_time_seconds > 0.0 &&
                                   options.max_time_seconds > 0.0
                               ? std::min(options.max_total_time_seconds,
                                          options.max_time_seconds)
                               : (options.max_total_time_seconds > 0.0
                                      ? options.max_total_time_seconds
                                      : options.max_time_seconds);
      if (limit > 0.0)
        deadline = std::chrono::steady_clock::now() +
                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                       std::chrono::duration<double>(limit));
    }
    marking_equation_ = marking_equation_describes_firing(net_);
    bool preflight_exhausted = false;
    if (const auto cost = shortest_model_run(options, deadline, preflight_exhausted)) {
      reachable_ = true;
      empty_run_cost_ = *cost;
    } else if (preflight_exhausted) {
      // Silent cycles that pump tokens (weighted arcs) make the uniform-cost
      // run unbounded; retry as A* over markings that can still reach the
      // final marking by the marking equation. Where the marking equation does
      // not describe firing (reset arcs, repeated weighted inputs) that A*
      // would be the same uniform-cost search again, so search breadth-first
      // by firing count instead, which a cost-0 pump cannot stall.
      preflight_exhausted = false;
      auto pruned = marking_equation_
                        ? pruned_model_run(options, deadline, preflight_exhausted)
                        : breadth_first_model_run(options, deadline, preflight_exhausted);
      // A reachability-only search (final_marking_reachability and
      // final_marking_reachable) whose pruned A* stopped at its budget tries
      // breadth-first too: a cost-0 silent pump that a silent drain can always
      // empty is not pruned by the marking equation and starves A*'s ties at
      // equal f (source → τ → source + sink, sink → τ, a: source + sink → sink;
      // review W1-3). Alignment preflights keep the A* outcome alone, and its
      // search counters with it. The run found is not necessarily the
      // cheapest, which a reachability verdict does not read.
      if (!pruned && preflight_exhausted && marking_equation_ && reachability_only) {
        preflight_exhausted = false;
        pruned = breadth_first_model_run(options, deadline, preflight_exhausted);
      }
      if (pruned) {
        reachable_ = true;
        empty_run_cost_ = *pruned;
      }
    }
    reachability_exhausted_ = preflight_exhausted;
  }

  // Whether the net's final marking is reachable from its initial marking — the
  // minimum bar for it to be a workflow net the aligner can score.
  [[nodiscard]] bool reachable() const { return reachable_; }

  [[nodiscard]] bool reachability_exhausted() const {
    return reachability_exhausted_;
  }

  [[nodiscard]] std::optional<std::size_t> activity_id(
      const std::string& name) const {
    const auto it = name_to_activity_.find(name);
    return it == name_to_activity_.end() ? std::nullopt
                                         : std::optional<std::size_t>(it->second);
  }

  // The fewest visible activities any complete model run fires — the model-move
  // half of the worst-case alignment cost used to normalize fitness.
  [[nodiscard]] std::uint64_t empty_run_cost() const { return empty_run_cost_; }

  [[nodiscard]] double weighted_empty_run_cost(const AlignmentOptions& options) const {
    const double model_default = options.default_model_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    const double silent_default = options.default_silent_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 1.0 : 0.0);
    auto transition_cost = [&](std::size_t t) {
      const auto& tr = net_.transitions()[t];
      const std::string name = tr.silent() ? std::string{} : net_.activities()[*tr.activity];
      auto it = options.model_costs.find(name);
      double value = tr.silent() ? silent_default
                                 : (it == options.model_costs.end() ? model_default : it->second);
      if (t < options.model_costs_by_transition.size()) value = options.model_costs_by_transition[t];
      if (options.model_cost_function) value = options.model_cost_function(name, t);
      return value;
    };
    // A cost callback is called per firing, as before; plain costs are
    // looked up once per transition.
    std::vector<double> plain_costs;
    if (!options.model_cost_function) {
      plain_costs.reserve(net_.transitions().size());
      for (std::size_t t = 0; t < net_.transitions().size(); ++t)
        plain_costs.push_back(transition_cost(t));
    }
    const auto cost_of = [&](std::size_t t) {
      return options.model_cost_function ? transition_cost(t) : plain_costs[t];
    };
    Marking next;  // reused firing buffer
    Marking start = initial_;
    std::priority_queue<std::pair<double, Marking>,
                        std::vector<std::pair<double, Marking>>,
                        std::greater<>> queue;
    std::unordered_map<Marking, double, MarkingHash> best{{start, 0.0}};
    queue.push({0.0, start});
    std::size_t expanded = 0;
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap : options.max_states;
    const auto started = std::chrono::steady_clock::now();
    CancelPoll cancelled(options.cancelled);
    while (!queue.empty() && expanded++ < budget) {
      if (cancelled()) return std::numeric_limits<double>::infinity();
      const double time_limit = options.max_total_time_seconds > 0.0 && options.max_time_seconds > 0.0
                                    ? std::min(options.max_total_time_seconds, options.max_time_seconds)
                                    : (options.max_total_time_seconds > 0.0 ? options.max_total_time_seconds
                                                                           : options.max_time_seconds);
      if (time_limit > 0.0 &&
          std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >=
              time_limit)
        return std::numeric_limits<double>::infinity();
      auto [cost, marking] = queue.top(); queue.pop();
      const auto found = best.find(marking);
      if (found == best.end() || cost > found->second + 1e-9) continue;
      if (marking == final_) return cost;
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!enabled(marking, t)) continue;
        const double next_cost = cost + cost_of(t);
        next = marking;
        fire_petri_transition(net_.transitions()[t], next);
        const auto old = best.find(next);
        if (old != best.end() && old->second <= next_cost + 1e-9) continue;
        if (old != best.end()) old->second = next_cost;
        else best.emplace(next, next_cost);
        queue.push({next_cost, next});
      }
    }
    return std::numeric_limits<double>::infinity();
  }

  struct Outcome {
    std::vector<AlignmentMove> moves;
    std::vector<AlignmentMove> complete_moves;
    double cost = 0.0;
    bool capped = false;  // search hit its bound before reaching the goal
    // The model marking the optimal alignment is in right before it consumes
    // log event d (after any model/silent moves scheduled first), for
    // d = 0..len-1, plus the final marking at index len. These are the states
    // alignment-based precision inspects. Empty when the search was capped.
    std::vector<Marking> prefix_markings;
    std::vector<ProjectedAlignmentMove> projected_model_moves;
  };

  // Optimal alignment of one trace (activity ids; std::nullopt = activity absent
  // from the model) carrying the matching activity names for display. Runs a
  // cheap plain-Dijkstra pass first — most traces align within a handful of
  // expansions and never pay for an LP — and only when its budget runs out
  // restarts with the marking-equation heuristic, whose pruning is what keeps
  // concurrent or badly fitting nets tractable.
  [[nodiscard]] Outcome align(
      const std::vector<std::optional<std::size_t>>& ids,
      const std::vector<std::string>& names) const {
    const std::size_t len = ids.size();
    const std::size_t pc = net_.place_count();
    const std::size_t tcount = net_.transitions().size();

    // A search state is the marking with the trace position appended at index
    // `pc`, so the same flat vector is both the search node and its hash-map key
    // -- no string is formatted and the marking is never copied just to key it.
    // enabled()/the firing below only ever touch place indices < pc, so the
    // appended position rides along untouched until a log or sync move bumps it.
    using State = Marking;
    State start_key = initial_;
    start_key.push_back(0);
    State final_key = final_;
    final_key.push_back(static_cast<int>(len));

    // Distinct model-known activities of the trace, with their transition
    // matches. Trace positions carrying the same activity are interchangeable
    // for the marking-equation relaxation, so the heuristic below groups them
    // by activity: its rows scale with the alphabet instead of the remaining
    // trace length, and its value depends only on the marking and the
    // remaining activity counts — which is what makes it cacheable across
    // states and across variants.
    std::vector<std::size_t> slot_activity;  // slot -> activity id
    std::vector<std::vector<std::size_t>> slot_matches;
    std::unordered_map<std::size_t, std::size_t> slot_of;
    std::vector<std::vector<std::size_t>> matches(len);
    for (std::size_t d = 0; d < len; ++d) {
      if (!ids[d]) continue;
      const auto [it, inserted] = slot_of.try_emplace(*ids[d], slot_activity.size());
      if (inserted) {
        slot_activity.push_back(*ids[d]);
        std::vector<std::size_t> ts;
        for (std::size_t t = 0; t < tcount; ++t) {
          const auto& transition = net_.transitions()[t];
          if (!transition.silent() && *transition.activity == *ids[d]) {
            ts.push_back(t);
          }
        }
        slot_matches.push_back(std::move(ts));
      }
      matches[d] = slot_matches[it->second];
    }
    const std::size_t slots = slot_activity.size();

    // suffix_counts[d][slot]: occurrences of the slot's activity in the
    // remaining trace ids[d..len). Canonical slot order (ascending activity
    // id) keys the cross-variant cache, since slots are discovered in
    // per-variant order.
    std::vector<std::vector<std::uint32_t>> suffix_counts(
        len + 1, std::vector<std::uint32_t>(slots, 0));
    for (std::size_t d = len; d-- > 0;) {
      suffix_counts[d] = suffix_counts[d + 1];
      if (ids[d]) ++suffix_counts[d][slot_of.at(*ids[d])];
    }
    std::vector<std::size_t> canonical_slots(slots);
    for (std::size_t i = 0; i < slots; ++i) canonical_slots[i] = i;
    std::sort(canonical_slots.begin(), canonical_slots.end(),
              [&](std::size_t a, std::size_t b) {
                return slot_activity[a] < slot_activity[b];
              });

    // Marking-equation heuristic: the remaining alignment cost is bounded
    // from below by relaxing it to an LP over continuous move counts x >= 0.
    // Columns are the net's transitions fired as model moves plus, per
    // remaining activity, one aggregated synchronous column per matching
    // transition and one slack; the activity's row caps synchronous use at
    // its remaining count, with the slack standing for the positions aligned
    // as log moves instead. Every remaining position pays a log move up
    // front (the `constant` below) and each synchronous unit refunds one, so
    // the aggregated optimum equals the per-position formulation's — the
    // positions it merges are interchangeable. Every real completion is a
    // feasible integer solution, so the LP optimum never overestimates;
    // costs are integral, so its ceiling is still a lower bound. Infeasible
    // means the final marking is unreachable from this state: the branch can
    // be pruned outright.
    struct LpBound {
      std::uint64_t bound = 0;
      bool solved = false;  // false: bound only (cache hit or aborted solve)
      std::vector<double> model_use;  // x_t per transition, when solved
      std::vector<double> sync_use;   // per matches[pos] entry, when solved
      double log_slack = 0.0;         // unmet remaining count of ids[pos]
      bool log_free = false;  // ids[pos] absent from the model: log is exact
    };
    const auto heuristic = [&](const State& state) -> std::optional<LpBound> {
      if (net_.has_extended_arcs() || !marking_equation_) return LpBound{};
      const std::size_t pos = static_cast<std::size_t>(state.back());
      const auto& counts = suffix_counts[pos];
      const double constant = static_cast<double>(len - pos);

      // The bound is a function of (marking, remaining counts) alone, so one
      // solve serves every state — of any variant — that shares them. Each
      // value packs into one byte (they are tiny in practice), spilling to an
      // escaped five-byte form when one ever is not — a prefix code, so the
      // fixed decode order (pc markings, then activity/count pairs) keeps the
      // key injective while millions of cached bounds stay affordable.
      std::string key;
      key.reserve(pc + slots * 2 + 4);
      const auto pack = [&key](std::uint32_t value) {
        if (value < 0xFE) {
          key += static_cast<char>(value);
        } else {
          key += static_cast<char>(0xFF);
          key += static_cast<char>(value & 0xFF);
          key += static_cast<char>((value >> 8) & 0xFF);
          key += static_cast<char>((value >> 16) & 0xFF);
          key += static_cast<char>((value >> 24) & 0xFF);
        }
      };
      for (std::size_t p = 0; p < pc; ++p) {
        pack(static_cast<std::uint32_t>(state[p]));
      }
      for (const std::size_t slot : canonical_slots) {
        if (counts[slot] == 0) continue;
        pack(static_cast<std::uint32_t>(slot_activity[slot]));
        pack(counts[slot]);
      }
      const auto cached = bound_cache_.find(key);
      LpBound result;
      if (cached != bound_cache_.end()) {
        if (!cached->second) return std::nullopt;  // known infeasible
        result.bound = *cached->second;
      } else {
        std::size_t active = 0;
        std::size_t sync_cols = 0;
        for (std::size_t slot = 0; slot < slots; ++slot) {
          if (counts[slot] == 0) continue;
          ++active;
          sync_cols += slot_matches[slot].size();
        }
        const std::size_t rows = pc + active;
        const std::size_t vars = tcount + sync_cols + active;

        std::vector<std::vector<double>> a(rows,
                                           std::vector<double>(vars, 0.0));
        std::vector<double> b(rows, 0.0);
        std::vector<double> c(vars, 0.0);
        for (std::size_t p = 0; p < pc; ++p) {
          b[p] = static_cast<double>(final_[p]) - static_cast<double>(state[p]);
        }
        for (std::size_t t = 0; t < tcount; ++t) {
          for (const auto& [place, coef] : incidence_[t]) a[place][t] = coef;
          c[t] = net_.transitions()[t].silent() ? 0.0 : 1.0;
        }
        std::size_t col = tcount;
        std::size_t row = pc;
        for (std::size_t slot = 0; slot < slots; ++slot) {
          if (counts[slot] == 0) continue;
          b[row] = static_cast<double>(counts[slot]);
          for (const std::size_t t : slot_matches[slot]) {
            for (const auto& [place, coef] : incidence_[t]) a[place][col] = coef;
            a[row][col] = 1.0;
            c[col] = -1.0;  // each synchronous unit refunds one paid log move
            ++col;
          }
          a[row][col] = 1.0;  // slack: positions left to align as log moves
          ++col;
          ++row;
        }

        auto lp = solve_lp(std::move(a), std::move(b), c);
        if (lp.aborted) return LpBound{};  // no usable bound, nothing dead
        if (!lp.feasible) {
          if (bound_cache_.size() < kAlignerBoundCacheCap) {
            bound_cache_.try_emplace(std::move(key), std::nullopt);
          }
          return std::nullopt;
        }
        result.bound = static_cast<std::uint64_t>(
            std::ceil(std::max(lp.objective + constant, 0.0) - 1e-6));
        if (bound_cache_.size() < kAlignerBoundCacheCap) {
          bound_cache_.try_emplace(std::move(key), result.bound);
        }

        // Unpack what the propagation needs for the current position.
        result.solved = true;
        result.model_use.assign(lp.solution.begin(),
                                lp.solution.begin() + tcount);
        if (pos < len && ids[pos]) {
          const std::size_t want = slot_of.at(*ids[pos]);
          std::size_t scan_col = tcount;
          for (std::size_t slot = 0; slot < slots; ++slot) {
            if (counts[slot] == 0) continue;
            if (slot == want) {
              result.sync_use.assign(
                  lp.solution.begin() + scan_col,
                  lp.solution.begin() + scan_col + slot_matches[slot].size());
              result.log_slack =
                  lp.solution[scan_col + slot_matches[slot].size()];
              break;
            }
            scan_col += slot_matches[slot].size() + 1;
          }
        }
      }
      if (pos < len && !ids[pos]) {
        // An activity the model lacks can only be a log move; consuming it
        // drops the constant by exactly one, so that step is always exact.
        result.log_free = true;
      }
      return result;
    };

    // A state whose marking equation is infeasible can never reach the goal;
    // an effectively infinite h keeps it buried without special-casing.
    constexpr std::uint64_t kDeadEnd =
        std::numeric_limits<std::uint64_t>::max() / 4;

    // One bounded best-first search; nullopt when the goal was not reached
    // within `budget` expansions. Without the LP every state is treated as
    // exactly bounded by zero, which is plain Dijkstra.
    const auto run = [&](bool use_lp,
                         std::size_t budget) -> std::optional<Outcome> {
      // States are interned: the hash map owns each state once and hands out a
      // dense id, so back-pointers, the frontier, and the cost table are plain
      // integers rather than copies of the state vector. Map nodes are stable,
      // so by_id can point into the map.
      struct Entry {
        std::uint64_t cost = 0;       // g: cheapest known cost to the state
        std::uint64_t heuristic = 0;  // h: admissible lower bound to the goal
        std::uint32_t prev = 0;
        AlignmentMoveType type = AlignmentMoveType::kModel;
        std::int32_t ref = -1;  // kLog: trace position; otherwise: activity id
        std::int32_t transition = -1;
        bool silent = false;
        bool exact = false;   // h is the LP value, not an inherited bound
        bool closed = false;  // expanded with an exact h: g is final
      };

      struct Node {
        std::uint64_t f;    // g + h at push time
        std::uint64_t g;
        std::uint64_t seq;  // FIFO tie-break keeps the search deterministic
        std::uint32_t state;
        bool operator>(const Node& other) const {
          if (f != other.f) return f > other.f;
          if (g != other.g) return g < other.g;  // prefer deeper states on ties
          return seq > other.seq;
        }
      };

      constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();
      std::priority_queue<Node, std::vector<Node>, std::greater<>> frontier;
      std::unordered_map<State, std::uint32_t, MarkingHash> index;
      std::vector<const State*> by_id;
      std::vector<Entry> entries;
      std::uint64_t seq = 0;
      std::uint32_t final_id = kNone;

      const auto intern = [&](State state) {
        const auto [it, inserted] =
            index.try_emplace(std::move(state), std::uint32_t(entries.size()));
        if (inserted) {
          by_id.push_back(&it->first);
          entries.emplace_back();
          if (it->first == final_key) final_id = it->second;
        }
        return std::make_pair(it->second, inserted);
      };

      // The start carries no bound yet: its first pop solves the LP (in LP
      // mode), so the expansion right after has the solution in hand.
      const std::uint32_t start_id = intern(start_key).first;
      entries[start_id].exact = !use_lp;
      frontier.push({0, 0, seq++, start_id});

      const auto relax = [&](State next, std::uint64_t cost, std::uint32_t prev,
                             AlignmentMoveType type, std::int32_t ref,
                             bool silent, std::uint64_t bound,
                             std::int32_t transition,
                             bool bound_exact) {
        const auto [id, inserted] = intern(std::move(next));
        Entry& entry = entries[id];
        // Inherited bounds are admissible for the child too, so keep whichever
        // lower bound is tighter; a bound the parent's LP solution walked is
        // the child's exact LP value and spares the child its own solve.
        if (bound > entry.heuristic) entry.heuristic = bound;
        if (bound_exact) entry.exact = true;
        if (!inserted && entry.cost <= cost) return;
        entry.cost = cost;
        entry.prev = prev;
        entry.type = type;
        entry.ref = ref;
        entry.transition = transition;
        entry.silent = silent;
        frontier.push({cost + entry.heuristic, cost, seq++, id});
      };

      bool reached = false;
      std::size_t expanded = 0;
      while (!frontier.empty() && expanded < budget) {
        const Node node = frontier.top();
        frontier.pop();
        {
          const Entry& entry = entries[node.state];
          if (node.g > entry.cost || entry.closed) continue;  // stale
          if (node.f < entry.cost + entry.heuristic) {
            // The bound tightened after this queue entry was pushed: re-queue
            // at the honest priority instead of expanding too early.
            frontier.push({entry.cost + entry.heuristic, node.g, seq++,
                           node.state});
            continue;
          }
        }

        if (node.state == final_id) {
          reached = true;
          break;
        }

        // Lazy exactness: states carry inherited bounds until they are
        // actually popped, so the LP runs only for states the search truly
        // visits — and the solution just computed marks the children on its
        // predicted path exact, sparing them their own solve. A bound served
        // from the cache carries no solution; its children simply inherit.
        LpBound info;
        if (!entries[node.state].exact) {
          auto solved = heuristic(*by_id[node.state]);
          Entry& entry = entries[node.state];
          entry.exact = true;
          if (!solved) {
            entry.heuristic = kDeadEnd;
            continue;
          }
          if (solved->bound > entry.heuristic) {
            entry.heuristic = solved->bound;
            if (entry.cost + entry.heuristic > node.f) {
              frontier.push({entry.cost + entry.heuristic, node.g, seq++,
                             node.state});
              continue;  // wait for its turn under the tightened bound
            }
          }
          info = std::move(*solved);
        }

        entries[node.state].closed = true;
        ++expanded;

        const std::uint64_t g = entries[node.state].cost;
        const std::uint64_t h = entries[node.state].heuristic;
        const std::uint64_t h_less_one = h > 0 ? h - 1 : 0;
        const State& state = *by_id[node.state];
        const std::size_t pos = static_cast<std::size_t>(state.back());
        // A move the LP optimum provably takes (the aggregated positions are
        // interchangeable, so "some remaining position of this activity"
        // stands for the current one) makes the child's inherited bound its
        // exact LP value, sparing the child its own solve.
        const auto walked_model = [&](std::size_t t) {
          return info.solved && info.model_use[t] >= 1.0 - 1e-6;
        };
        const auto walked_sync = [&](std::size_t k) {
          return info.solved && k < info.sync_use.size() &&
                 info.sync_use[k] >= 1.0 - 1e-6;
        };
        const bool walked_log =
            info.log_free || (info.solved && info.log_slack >= 1.0 - 1e-6);

        // Log move: the trace advances while the model stands still.
        if (pos < len) {
          State next = state;
          next.back() = static_cast<int>(pos + 1);
          relax(std::move(next), g + 1, node.state, AlignmentMoveType::kLog,
                static_cast<std::int32_t>(pos), false, h_less_one,
                -1, !use_lp || walked_log);
        }

        // Model moves (and the synchronous move when the labels agree).
        for (std::size_t t = 0; t < tcount; ++t) {
          if (!enabled(state, t)) continue;
          const auto& transition = net_.transitions()[t];
          State fired = fire(state, t);
          if (transition.silent()) {
            relax(std::move(fired), g, node.state, AlignmentMoveType::kModel,
                  -1, true, h, static_cast<std::int32_t>(t), !use_lp || walked_model(t));
            continue;
          }
          const auto activity = static_cast<std::int32_t>(*transition.activity);
          const bool sync = pos < len && ids[pos] &&
                            *ids[pos] == *transition.activity;
          if (sync) {
            const std::size_t k = static_cast<std::size_t>(
                std::find(matches[pos].begin(), matches[pos].end(), t) -
                matches[pos].begin());
            State synced = fired;
            synced.back() = static_cast<int>(pos + 1);
            relax(std::move(fired), g + 1, node.state,
                  AlignmentMoveType::kModel, activity, false, h_less_one,
                  static_cast<std::int32_t>(t), !use_lp || walked_model(t));
            relax(std::move(synced), g, node.state,
                  AlignmentMoveType::kSynchronous, activity, false, h,
                  static_cast<std::int32_t>(t), !use_lp || walked_sync(k));
          } else {
            relax(std::move(fired), g + 1, node.state,
                  AlignmentMoveType::kModel, activity, false, h_less_one,
                  static_cast<std::int32_t>(t), !use_lp || walked_model(t));
          }
        }
      }

      if (!reached) return std::nullopt;

      Outcome outcome;
      outcome.cost = entries[final_id].cost;
      std::vector<std::uint32_t> path{final_id};
      for (std::uint32_t id = final_id; id != start_id;) {
        const Entry& entry = entries[id];
        const AlignmentMove move{
            entry.type,
            entry.silent
                ? std::string{}
                : (entry.type == AlignmentMoveType::kLog
                       ? names[static_cast<std::size_t>(entry.ref)]
                       : net_.activities()[static_cast<std::size_t>(entry.ref)]),
            entry.transition < 0 ? static_cast<std::size_t>(-1)
                                 : static_cast<std::size_t>(entry.transition)};
        outcome.complete_moves.push_back(move);
        if (!entry.silent) outcome.moves.push_back(move);
        id = entry.prev;
        path.push_back(id);
      }
      std::reverse(outcome.complete_moves.begin(), outcome.complete_moves.end());
      std::reverse(outcome.moves.begin(), outcome.moves.end());
      std::reverse(path.begin(), path.end());

      // Prefix markings for alignment-based precision: along the optimal path,
      // the trace position advances exactly once per consumed log event, and
      // the state it advances from holds the model marking at that moment.
      const auto strip = [](const State& state) {
        return Marking(state.begin(), state.end() - 1);
      };
      outcome.prefix_markings.resize(len + 1);
      for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const State& state = *by_id[path[i]];
        const State& next = *by_id[path[i + 1]];
        if (next.back() == state.back() + 1) {
          outcome.prefix_markings[static_cast<std::size_t>(state.back())] =
              strip(state);
        }
      }
      outcome.prefix_markings[len] = strip(*by_id[path.back()]);
      for (std::size_t i=0;i+1<path.size();++i) {
        const auto& e=entries[path[i+1]]; if (e.silent || e.type==AlignmentMoveType::kLog || e.transition<0) continue;
        outcome.projected_model_moves.push_back({net_.activities()[static_cast<std::size_t>(e.ref)], static_cast<std::size_t>(e.transition), strip(*by_id[path[i]]), strip(*by_id[path[i+1]])});
      }
      return outcome;
    };

    const std::size_t quick =
        std::min<std::size_t>(kAlignSearchCap, 8 * (len + 1) + 256);
    if (auto outcome = run(false, quick)) return *outcome;
    if (auto outcome = run(true, kAlignSearchCap)) return *outcome;

    // Search capped out: fall back to the worst-case alignment so callers
    // still get a well-formed result, and flag that the numbers are not
    // trustworthy.
    Outcome outcome;
    for (std::size_t i = 0; i < len; ++i) {
      const AlignmentMove move{AlignmentMoveType::kLog, names[i]};
      outcome.moves.push_back(move);
      outcome.complete_moves.push_back(move);
    }
    outcome.cost = static_cast<std::uint64_t>(len) + empty_run_cost_;
    outcome.capped = true;
    return outcome;
  }

  struct ConfiguredOutcome {
    Outcome primary;
    std::vector<std::vector<AlignmentMove>> alternatives;
    AlignmentDiagnostics diagnostics;
    bool invalid_cost = false;
  };

  // Clean-room implementation of the Dijkstra/generator/discounted
  // variants.  The search deliberately keeps the product state explicit and
  // uses floating point edge costs, which is required by discounted A* and
  // custom cost functions.
  [[nodiscard]] ConfiguredOutcome align_configured(
      const std::vector<std::optional<std::size_t>>& ids,
      const std::vector<std::string>& names,
      const AlignmentOptions& input_options) const {
    AlignmentOptions options = input_options;
    bool callback_invalid = false;
    // Resolve callbacks exactly once for this concrete trace/model pair. The
    // search and LP then consume stable vectors, avoiding repeated/stateful
    // callback evaluation and making invalid values deterministic.
    if (options.model_cost_function || options.sync_cost_function) {
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        const auto& tr = net_.transitions()[t];
        const std::string name = tr.silent() ? std::string{} : net_.activities()[*tr.activity];
        if (options.model_cost_function) {
          const double value = options.model_cost_function(name, t);
          if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
          options.model_costs_by_transition.resize(net_.transitions().size());
          options.model_costs_by_transition[t] = value;
        }
        if (options.sync_cost_function && !tr.silent()) {
          const double value = options.sync_cost_function(name, t);
          if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
          options.sync_costs_by_transition.resize(net_.transitions().size());
          options.sync_costs_by_transition[t] = value;
        }
      }
      options.model_cost_function = {};
      options.sync_cost_function = {};
    }
    if (options.trace_cost_function) {
      options.trace_costs_by_position.resize(names.size());
      for (std::size_t i = 0; i < names.size(); ++i) {
        options.trace_costs_by_position[i] = options.trace_cost_function(names[i], i);
        if (!std::isfinite(options.trace_costs_by_position[i]) || options.trace_costs_by_position[i] < 0.0)
          callback_invalid = true;
      }
      options.trace_cost_function = {};
    }
    if (callback_invalid) {
      ConfiguredOutcome invalid;
      invalid.primary.capped = true;
      invalid.invalid_cost = true;
      return invalid;
    }
    using Clock = std::chrono::steady_clock;
    const bool generator =
        options.variant == AlignmentVariant::kGeneratorDijkstraNoHeuristics ||
        options.variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
    const bool less_memory =
        options.variant == AlignmentVariant::kDijkstraLessMemory ||
        options.variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
    const bool discounted = options.variant == AlignmentVariant::kDiscountedAStar;
    const double exponent = options.exponent > 1e-9 ? options.exponent : 1.0;
    const double default_trace = options.default_trace_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    const double default_model = options.default_model_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    const double default_sync = options.default_sync_cost.value_or(0.0);
    const double default_silent = options.default_silent_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 1.0 : 0.0);
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap
                                                        : options.max_states;
    const std::size_t alignment_limit = options.max_alignments == 0 ? 32 : options.max_alignments;
    const std::size_t len = ids.size();
    const std::size_t pc = net_.place_count();
    using State = Marking;
    State start = initial_;
    start.push_back(0);
    State goal = final_;
    goal.push_back(static_cast<int>(len));

    // Search paths are stored once, as parent-linked records; a queued node
    // refers to the record of its last step. Copying whole paths into every
    // queued node made memory grow with queue size times path length, so long
    // traces hit the stored-path guard after a few tens of thousands of states.
    constexpr std::size_t kNoRecord = static_cast<std::size_t>(-1);
    struct PathRecord {
      std::size_t parent = kNoRecord;
      State state;
      AlignmentMove move{};
      bool has_move = false;  // the step is listed among the alignment moves
      bool consumed = false;  // the step consumed a log event
    };
    std::vector<PathRecord> records;
    struct Node {
      double cost = 0.0;
      double priority = 0.0;
      std::uint64_t seq = 0;
      std::size_t depth = 0;
      bool discounted_order = false;
      std::size_t record = 0;  // its state is records[record].state
      // Queued with a priority inherited from its parent's bound; the
      // state's own bound is solved when the node first reaches the top.
      bool pending = false;
      bool operator>(const Node& o) const {
        if (priority != o.priority) return priority > o.priority;
        if (cost != o.cost) return cost > o.cost;
        if (discounted_order && depth != o.depth) return depth < o.depth;
        return seq > o.seq;
      }
    };
    // A binary heap on std::greater, popped exactly as std::priority_queue
    // pops (pop_heap, then the back), so the node can be moved out.
    std::vector<Node> frontier;
    const auto frontier_push = [&](Node node) {
      frontier.push_back(node);
      std::push_heap(frontier.begin(), frontier.end(), std::greater<>{});
    };
    // The discounted variant closes product states (marking + trace
    // position), deliberately ignoring path depth.  This keeps cyclic models
    // finite and deterministic rather than chasing an unattained discounted
    // infimum through repeated positive-cost cycles.
    std::unordered_map<State, double, MarkingHash> best;
    // Keyed by the marking part of a state; looked up by a view of the state
    // so a known marking is never copied.
    std::unordered_map<Marking, std::vector<std::pair<std::size_t, double>>,
                       MarkingPrefixHash, MarkingPrefixEqual>
        pareto_positions;
    best.emplace(start, 0.0);
    std::size_t lp_solved = 0;
    const auto lookup_cost = [](const std::unordered_map<std::string, double>& table,
                                const std::string& name, double fallback) {
      const auto it = table.find(name);
      return it == table.end() ? fallback : it->second;
    };
    const auto model_cost_for = [&](std::size_t t) {
      const auto& tr = net_.transitions()[t];
      const std::string name = tr.silent() ? std::string{} : net_.activities()[*tr.activity];
      const auto found = options.model_costs.find(name);
      double value = tr.silent()
                         ? default_silent
                         : (found == options.model_costs.end() ? default_model : found->second);
      if (t < options.model_costs_by_transition.size()) value = options.model_costs_by_transition[t];
      if (options.model_cost_function) value = options.model_cost_function(name, t);
      return value;
    };
    const auto trace_cost_for = [&](std::size_t d) {
      const std::string& name = names[d];
      double value = lookup_cost(options.trace_costs, name, default_trace);
      if (d < options.trace_costs_by_position.size()) value = options.trace_costs_by_position[d];
      if (options.trace_cost_function) value = options.trace_cost_function(name, d);
      return value;
    };
    const auto sync_cost_for = [&](std::size_t t) {
      const std::string& name = net_.activities()[*net_.transitions()[t].activity];
      double value = lookup_cost(options.sync_costs, name, default_sync);
      if (t < options.sync_costs_by_transition.size()) value = options.sync_costs_by_transition[t];
      if (options.sync_cost_function) value = options.sync_cost_function(name, t);
      return value;
    };
    // A state's bound depends on nothing but the state (the trace and the
    // costs are fixed for the search), so each is solved once.
    std::unordered_map<State, double, MarkingHash> bounds;
    const auto lp_heuristic = [&](const State& state) {
      // The marking-equation bound is in undiscounted units and may
      // overestimate a discounted suffix.  Discounted search therefore uses
      // a zero heuristic (Dijkstra), preserving optimal goal termination.
      if (net_.has_extended_arcs() || !marking_equation_ ||
          options.variant != AlignmentVariant::kStateEquationAStar || discounted)
        return 0.0;
      if (const auto known = bounds.find(state); known != bounds.end()) return known->second;
      const auto solved = [&]() -> double {
        const std::size_t pos = static_cast<std::size_t>(state.back());
        const std::size_t remaining = len - pos;
        const std::size_t tcount = net_.transitions().size();
        std::size_t sync_count = 0;
        for (std::size_t d = pos; d < len; ++d) {
          if (!ids[d]) continue;
          for (std::size_t t = 0; t < tcount; ++t)
            if (!net_.transitions()[t].silent() && *net_.transitions()[t].activity == *ids[d]) ++sync_count;
        }
        const std::size_t vars = tcount + remaining + sync_count;
        const std::size_t rows = pc + remaining;
        std::vector<std::vector<double>> a(rows, std::vector<double>(vars, 0.0));
        std::vector<double> b(rows, 0.0), c(vars, 0.0);
        for (std::size_t p = 0; p < pc; ++p) b[p] = double(final_[p] - state[p]);
        for (std::size_t t = 0; t < tcount; ++t) {
          for (const auto& [p, coef] : incidence_[t]) a[p][t] = coef;
          c[t] = model_cost_for(t);
        }
        std::size_t col = tcount;
        for (std::size_t d = pos; d < len; ++d) {
          const std::size_t row = pc + (d - pos);
          b[row] = 1.0;
          a[row][col] = 1.0;
          c[col] = trace_cost_for(d);
          ++col;
          if (!ids[d]) continue;
          for (std::size_t t = 0; t < tcount; ++t) {
            if (net_.transitions()[t].silent() || *net_.transitions()[t].activity != *ids[d]) continue;
            for (const auto& [p, coef] : incidence_[t]) a[p][col] = coef;
            a[row][col] = 1.0;
            c[col] = sync_cost_for(t);
            ++col;
          }
        }
        ++lp_solved;
        auto lp = solve_lp(std::move(a), std::move(b), c);
        if (lp.aborted) return 0.0;
        if (!lp.feasible) return std::numeric_limits<double>::infinity();
        return std::max(0.0, lp.objective);
      }();
      bounds.emplace(state, solved);
      return solved;
    };
    // State-equation A* defers a successor's bound until the successor
    // reaches the top of the queue.  It is queued with the bound its parent
    // implies: the marking-equation bound is consistent (a successor's LP
    // solution plus the move solves the parent's LP, so the parent's bound is
    // at most the successor's bound plus the move's cost), which makes the
    // inherited priority a lower bound of the exact one.  A pending node that
    // reaches the top is re-queued with its exact priority and keeps its
    // sequence number, so nodes are expanded in the order eager bounds give
    // them, and no LP is solved for successors the search never reaches.
    const bool lazy_bounds = options.variant == AlignmentVariant::kStateEquationAStar &&
                             !generator && !discounted && !less_memory &&
                             marking_equation_ && !net_.has_extended_arcs();
    records.push_back(PathRecord{kNoRecord, start, {}, false, false});
    frontier_push(Node{0.0, lp_heuristic(start), 0, 0, discounted, 0});
    struct GoalPath {
      double cost = 0.0;
      std::vector<AlignmentMove> moves;
      std::vector<Marking> prefixes;
      std::vector<State> path_states;
    };
    const auto reconstruct = [&](const Node& last) {
      std::vector<std::size_t> chain;
      for (std::size_t r = last.record; r != kNoRecord; r = records[r].parent)
        chain.push_back(r);
      std::reverse(chain.begin(), chain.end());
      GoalPath path;
      path.cost = last.cost;
      for (const std::size_t r : chain) {
        const PathRecord& record = records[r];
        path.path_states.push_back(record.state);
        if (record.parent == kNoRecord) continue;
        if (record.consumed) {
          const State& before = records[record.parent].state;
          path.prefixes.push_back(Marking(before.begin(), before.end() - 1));
        }
        if (record.has_move) path.moves.push_back(record.move);
      }
      return path;
    };
    const auto path_contains = [&](std::size_t record, const State& state) {
      for (std::size_t r = record; r != kNoRecord; r = records[r].parent)
        if (records[r].state == state) return true;
      return false;
    };
    std::unordered_set<State, MarkingHash> discounted_closed;
    std::unordered_map<State, std::unordered_set<std::string>, MarkingHash>
        generator_seen;
    const auto start_time = Clock::now();
    std::size_t seq = 1;
    std::size_t expanded = 0;
    std::size_t arcs = 0;
    bool invalid_cost = false;
    std::size_t queued_total = 0;
    // Every queued node adds one path record. Unbounded nets whose discounted
    // edge costs vanish with depth would otherwise exhaust memory long before
    // the expansion budget; past this many stored path records the search
    // gives up as capped.
    constexpr std::size_t kStoredPathEntryCap = 5'000'000;
    double goal_cost = std::numeric_limits<double>::infinity();
    std::vector<GoalPath> goals;
    const auto move_key = [](const std::vector<AlignmentMove>& moves) {
      std::string key;
      for (const auto& m : moves) {
        key.push_back(static_cast<char>('0' + static_cast<int>(m.type)));
        key += std::to_string(m.transition);
        key.push_back(':');
        key += m.activity;
        key.push_back(';');
      }
      return key;
    };
    CancelPoll cancelled(options.cancelled);
    const auto timed_out = [&]() {
      return (options.max_time_seconds > 0.0 &&
              std::chrono::duration<double>(Clock::now() - start_time).count() >=
                  options.max_time_seconds) ||
             cancelled();
    };
    State node_state, fired, synced;  // reused buffers
    while (!frontier.empty() && expanded < budget && !timed_out() &&
           records.size() < kStoredPathEntryCap) {
      std::pop_heap(frontier.begin(), frontier.end(), std::greater<>{});
      Node node = frontier.back();
      frontier.pop_back();
      node_state = records[node.record].state;
      const auto it_best = best.find(node_state);
      if (it_best == best.end() || node.cost > it_best->second + 1e-9) continue;
      if (node.pending) {
        const double bound = lp_heuristic(node_state);
        if (!std::isfinite(bound)) continue;
        // An aborted LP reports 0; the inherited bound stays valid then.
        node.priority = std::max(node.priority, node.cost + bound);
        node.pending = false;
        frontier_push(node);
        continue;
      }
      if (discounted && !discounted_closed.insert(node_state).second) continue;
      if (!generator && node_state == goal) {
        GoalPath path = reconstruct(node);
        if (path.prefixes.size() == len)
          path.prefixes.push_back(Marking(final_.begin(), final_.end()));
        goals.push_back(std::move(path));
        goal_cost = goals.front().cost;
        break;
      }
      if (generator && node_state == goal) {
        if (node.cost > goal_cost + 1e-9) break;
        if (!std::isfinite(goal_cost)) goal_cost = node.cost;
        GoalPath path = reconstruct(node);
        if (path.prefixes.size() == len)
          path.prefixes.push_back(Marking(final_.begin(), final_.end()));
        const auto key = move_key(path.moves);
        auto& seen = generator_seen[node_state];
        if (seen.insert(key).second && goals.size() < alignment_limit) {
          goals.push_back(std::move(path));
        }
        if (goals.size() >= alignment_limit) break;
        continue;
      }
      ++expanded;
      const std::size_t pos = static_cast<std::size_t>(node_state.back());
      const double factor = discounted
                                ? std::pow(exponent, -static_cast<double>(node.depth))
                                : 1.0;
      // `child_state` is only read; the record copies it when it is queued.
      auto push = [&](const State& child_state, double edge_cost, AlignmentMove move,
                      bool consume) {
        ++arcs;
        if (!std::isfinite(edge_cost) || edge_cost < 0.0) {
          invalid_cost = true;
          return;
        }
        Node child;
        child.discounted_order = node.discounted_order;
        if (discounted && discounted_closed.find(child_state) != discounted_closed.end()) return;
        if (generator && path_contains(node.record, child_state)) return;
        child.depth = node.depth + 1;
        child.seq = seq++;
        child.cost = node.cost + edge_cost * factor;
        if (!lazy_bounds) {
          child.priority = child.cost + lp_heuristic(child_state);
          if (!std::isfinite(child.priority)) return;
        }
        // Configured variants retain tau/model routing moves explicitly;
        // an empty activity denotes a silent transition.
        const bool keep_move =
            !move.activity.empty() || move.type == AlignmentMoveType::kModel;
        const auto enqueue = [&]() {
          records.push_back(PathRecord{node.record, child_state, std::move(move),
                                       keep_move, consume});
          child.record = records.size() - 1;
          frontier_push(child);
          ++queued_total;
        };
        if (less_memory) {
          const MarkingPrefix marking{child_state.data(), child_state.size() - 1};
          const std::size_t child_pos = static_cast<std::size_t>(child_state.back());
          auto found_points = pareto_positions.find(marking);
          if (found_points == pareto_positions.end())
            found_points = pareto_positions.emplace(
                Marking(child_state.begin(), child_state.end() - 1),
                std::vector<std::pair<std::size_t, double>>{}).first;
          auto& frontier_points = found_points->second;
          for (const auto& [prior_pos, prior_cost] : frontier_points)
            if ((!generator || prior_pos > child_pos || prior_cost < child.cost - 1e-9) &&
                prior_pos >= child_pos && prior_cost <= child.cost + 1e-9) return;
          frontier_points.erase(
              std::remove_if(frontier_points.begin(), frontier_points.end(),
                             [&](const auto& point) {
                               return child_pos >= point.first &&
                                      child.cost <= point.second + 1e-9;
                             }),
              frontier_points.end());
          frontier_points.emplace_back(child_pos, child.cost);
        }
        const auto old = best.find(child_state);
        const auto record_best = [&] {
          if (old != best.end()) old->second = child.cost;
          else best.emplace(child_state, child.cost);
        };
        if (!generator) {
          if (old != best.end()) {
            if (child.cost > old->second + 1e-9) return;
            if (!discounted && old->second <= child.cost + 1e-9) return;
            if (child.cost >= old->second - 1e-9) {
              // Discounted equal-cost arrivals remain queued so depth tie
              // ordering can select the deeper pinned path.
              enqueue();
              return;
            }
          }
          if (lazy_bounds) {
            if (const auto known = bounds.find(child_state); known != bounds.end()) {
              if (!std::isfinite(known->second)) return;
              child.priority = child.cost + known->second;
            } else {
              // The parent's bound less the edge cost, with a margin for LP
              // rounding so the inherited priority never exceeds the exact one.
              const double parent_bound = node.priority - node.cost;
              const double inherited =
                  parent_bound - edge_cost - 1e-6 * std::max(1.0, std::abs(parent_bound));
              child.priority = child.cost + std::max(0.0, inherited);
              child.pending = true;
            }
          }
          record_best();
        } else if (old == best.end() || child.cost < old->second - 1e-9) {
          record_best();
        } else if (child.cost > old->second + 1e-9) {
          return;
        }
        enqueue();
      };
      // Explicit mode enumerates the raw log successor first, matching the
      // pinned non-synchronous product ordering; compact mode emits it after
      // model successors so synchronous destinations are coalesced eagerly.
      if (!options.synchronous_dijkstra && pos < len) {
        synced = node_state;
        synced.back() = static_cast<int>(pos + 1);
        const std::string& activity = names[pos];
        double trace_cost = lookup_cost(options.trace_costs, activity, default_trace);
        if (pos < options.trace_costs_by_position.size()) trace_cost = options.trace_costs_by_position[pos];
        if (options.trace_cost_function) trace_cost = options.trace_cost_function(activity, pos);
        push(synced, trace_cost, {AlignmentMoveType::kLog, activity}, true);
      }
      for (std::size_t order = 0; order < net_.transitions().size(); ++order) {
        const std::size_t t = options.synchronous_dijkstra
                                  ? order
                                  : net_.transitions().size() - 1 - order;
        if (!enabled(node_state, t)) continue;
        const auto& tr = net_.transitions()[t];
        fired = node_state;
        fire_petri_transition(tr, fired);
        if (tr.silent()) {
          push(fired, model_cost_for(t),
               {AlignmentMoveType::kModel, {}, t}, false);
          continue;
        }
        const std::string& activity = net_.activities()[*tr.activity];
        double model_cost = lookup_cost(options.model_costs, activity, default_model);
        if (t < options.model_costs_by_transition.size())
          model_cost = options.model_costs_by_transition[t];
        if (options.model_cost_function) model_cost = options.model_cost_function(activity, t);
        push(fired, model_cost,
             {AlignmentMoveType::kModel, activity, t}, false);
        if (pos < len && ids[pos] && *ids[pos] == *tr.activity) {
          synced = fired;
          synced.back() = static_cast<int>(pos + 1);
          double sync_cost = lookup_cost(options.sync_costs, activity, default_sync);
          if (t < options.sync_costs_by_transition.size())
            sync_cost = options.sync_costs_by_transition[t];
          if (options.sync_cost_function) sync_cost = options.sync_cost_function(activity, t);
          push(synced, sync_cost,
               {AlignmentMoveType::kSynchronous, activity, t}, true);
        }
      }
      if (options.synchronous_dijkstra && pos < len) {
        synced = node_state;
        synced.back() = static_cast<int>(pos + 1);
        const std::string& activity = names[pos];
        double trace_cost = lookup_cost(options.trace_costs, activity, default_trace);
        if (pos < options.trace_costs_by_position.size())
          trace_cost = options.trace_costs_by_position[pos];
        if (options.trace_cost_function) trace_cost = options.trace_cost_function(activity, pos);
        push(synced, trace_cost,
             {AlignmentMoveType::kLog, activity}, true);
      }
    }
    ConfiguredOutcome result;
    result.diagnostics.visited_states = expanded;
    result.diagnostics.queued_states = queued_total;
    result.diagnostics.traversed_arcs = arcs;
    result.diagnostics.lp_solved = lp_solved;
    result.diagnostics.closed_set_length = best.size();
    result.diagnostics.num_visited_markings = result.diagnostics.closed_set_length;
    if (goals.empty() || invalid_cost) {
      result.primary.capped = true;
      result.invalid_cost = invalid_cost;
      return result;
    }
    auto& primary = goals.front();
    result.primary.cost = primary.cost;
    for (std::size_t i=0; i<primary.moves.size() && i+1<primary.path_states.size(); ++i) {
      const auto& mv=primary.moves[i];
      if (mv.activity.empty() || mv.type==AlignmentMoveType::kLog || mv.transition==static_cast<std::size_t>(-1)) continue;
      auto before=primary.path_states[i]; if(!before.empty()) before.pop_back();
      auto after=primary.path_states[i+1]; if(!after.empty()) after.pop_back();
      result.primary.projected_model_moves.push_back({mv.activity,mv.transition,
        std::move(before), std::move(after)});
    }
    result.primary.moves = std::move(primary.moves);
    result.primary.complete_moves = result.primary.moves;
    result.primary.prefix_markings = std::move(primary.prefixes);
    for (std::size_t i = 1; i < goals.size(); ++i)
      result.alternatives.push_back(std::move(goals[i].moves));
    return result;
  }

 private:
  [[nodiscard]] bool enabled(const Marking& marking, std::size_t t) const {
    return petri_transition_enabled(net_.transitions()[t], marking);
  }

  [[nodiscard]] Marking fire(const Marking& marking, std::size_t t) const {
    Marking next = marking;
    fire_petri_transition(net_.transitions()[t], next);
    return next;
  }

  // Cheapest count of visible transitions on any run from the initial to the
  // final marking, found by the same uniform-cost search with no trace, or
  // std::nullopt when the final marking is unreachable within the search bound.
  [[nodiscard]] std::optional<std::uint64_t> shortest_model_run(
      const AlignmentOptions& options,
      std::chrono::steady_clock::time_point deadline,
      bool& exhausted) const {
    constexpr std::uint64_t kUnseen = std::numeric_limits<std::uint64_t>::max();
    MarkingTable table(initial_.size());
    std::vector<std::uint64_t> best;  // by marking id; kUnseen when absent
    using Entry = std::pair<std::uint64_t, std::uint32_t>;  // cost, marking id
    // Orders like std::greater over (cost, Marking).
    const auto later = [&](const Entry& left, const Entry& right) {
      if (left.first != right.first) return left.first > right.first;
      return table.greater(left.second, right.second);
    };
    std::priority_queue<Entry, std::vector<Entry>, decltype(later)> frontier(later);
    const auto start = table.intern(initial_).first;
    best.push_back(0);
    frontier.push({0, start});
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap
                                                        : options.max_states;
    std::size_t expanded = 0;
    CancelPoll cancelled(options.cancelled);
    Marking current, successor;
    std::vector<std::size_t> fired;
    std::vector<int> successors;
    while (!frontier.empty() && expanded < budget) {
      if (std::chrono::steady_clock::now() >= deadline || cancelled()) {
        exhausted = true;
        return std::nullopt;
      }
      ++expanded;
      const auto [cost, id] = frontier.top();
      frontier.pop();
      if (best[id] != kUnseen && cost > best[id]) continue;
      table.copy(id, current);
      if (current == final_) return cost;
      // The successors in transition order, interned as one batch.
      fired.clear();
      successors.clear();
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!enabled(current, t)) continue;
        successor = current;
        fire_petri_transition(net_.transitions()[t], successor);
        fired.push_back(t);
        successors.insert(successors.end(), successor.begin(), successor.end());
      }
      table.intern_batch(successors.data(), fired.size(), [&](std::size_t k, std::uint32_t next, bool added) {
        const std::uint64_t next_cost =
            cost + (net_.transitions()[fired[k]].silent() ? 0 : 1);
        if (added) best.push_back(kUnseen);
        if (best[next] != kUnseen && best[next] <= next_cost) return;
        best[next] = next_cost;
        frontier.push({next_cost, next});
      });
    }
    exhausted = !frontier.empty();
    return std::nullopt;  // final marking unreachable: the net is not a sound WF-net
  }

  // Reachability of the final marking by a breadth-first search over firing
  // counts, for nets whose marking equation does not describe firing (no
  // admissible bound to prune with). A uniform-cost search stalls on an
  // unbounded cost-0 silent pump; breadth-first order still reaches a final
  // marking a few firings away. Returns the visible count of the first run
  // found (not necessarily the cheapest), std::nullopt when the final marking
  // is unreachable or the search stopped at its budget (`exhausted`).
  [[nodiscard]] std::optional<std::uint64_t> breadth_first_model_run(
      const AlignmentOptions& options,
      std::chrono::steady_clock::time_point deadline,
      bool& exhausted) const {
    MarkingTable table(initial_.size());
    std::vector<std::uint64_t> cost;  // by marking id: visible count on its first run
    std::deque<std::uint32_t> frontier;
    frontier.push_back(table.intern(initial_).first);
    cost.push_back(0);
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap
                                                        : options.max_states;
    std::size_t expanded = 0;
    CancelPoll cancelled(options.cancelled);
    Marking current, successor;
    std::vector<std::size_t> fired;
    std::vector<int> successors;
    while (!frontier.empty() && expanded < budget) {
      if (std::chrono::steady_clock::now() >= deadline || cancelled()) {
        exhausted = true;
        return std::nullopt;
      }
      ++expanded;
      const auto id = frontier.front();
      frontier.pop_front();
      table.copy(id, current);
      if (current == final_) return cost[id];
      fired.clear();
      successors.clear();
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!enabled(current, t)) continue;
        successor = current;
        fire_petri_transition(net_.transitions()[t], successor);
        fired.push_back(t);
        successors.insert(successors.end(), successor.begin(), successor.end());
      }
      const auto base = cost[id];
      table.intern_batch(successors.data(), fired.size(), [&](std::size_t k, std::uint32_t next, bool added) {
        if (!added) return;
        cost.push_back(base + (net_.transitions()[fired[k]].silent() ? 0 : 1));
        frontier.push_back(next);
      });
    }
    exhausted = !frontier.empty();
    return std::nullopt;
  }

  // shortest_model_run as an A* whose heuristic is the marking-equation bound
  // on the visible transitions still to fire; markings from which the final
  // marking is unreachable by the marking equation are dropped.
  //
  // The bound of a successor is solved lazily. A successor enters the
  // frontier as a pending entry keyed (g + l, g, marking), where l is a lower
  // bound on its marking-equation bound h (0, or a dual bound below), and its
  // LP is solved only when the pending entry reaches the top: it is dropped
  // there (an infeasible marking, which the eager search never queues) or
  // queued again with its exact key (g + h, g, marking). Keys are distinct (a
  // marking is queued again only at a strictly lower g), so whenever an exact
  // entry is on top it is the least key the eager search (every bound solved
  // when its successor is generated) would hold: both expand the same entries
  // in the same order, count the same expansions against the budget and
  // return the same cost and `exhausted`, while the LPs of the successors a
  // search stopped by its budget never reaches are never solved. Each
  // marking's bound is still solved at most once.
  //
  // The dual bound: the optimal dual y of any solved program is feasible for
  // all of them (c - yA >= 0 does not involve b), so y·(final - m) <= h(m)
  // for every marking m by weak duality. The duals of solves whose pending
  // bound fell short are kept (up to kDualPool), and a successor's l is the
  // best of them, rounded down with a margin far beyond the solver's
  // tolerance, so l never exceeds the rounded-up bound the LP gives.
  [[nodiscard]] std::optional<std::uint64_t> pruned_model_run(
      const AlignmentOptions& options,
      std::chrono::steady_clock::time_point deadline,
      bool& exhausted) const {
    const std::size_t tcount = net_.transitions().size();
    const std::size_t places = initial_.size();
    // The marking equation describes firing only when every firing adds its
    // incidence column (marking_equation_describes_firing). The constructor
    // runs this search only then; the guard keeps the bound sound for any
    // other caller: without it no LP bound is applied (h = 0, nothing
    // pruned), and with it no parent-plan or dual bound.
    const bool marking_equation = marking_equation_;
    std::vector<double> c(tcount, 0.0);
    for (std::size_t t = 0; t < tcount; ++t) c[t] = net_.transitions()[t].silent() ? 0.0 : 1.0;
    // The incidence matrix as sparse rows; only b changes between markings.
    std::vector<double> dense(places * tcount, 0.0);
    for (std::size_t t = 0; t < tcount; ++t)
      for (const auto& [p, coef] : incidence_[t]) dense[p * tcount + t] = coef;
    const auto a = lp_rows([&](std::size_t i, std::size_t j) { return dense[i * tcount + j]; },
                           places, tcount);
    std::vector<double> dual;
    // Integral optimal solutions (transition, count) of recent markings.
    using Plan = std::vector<std::pair<std::uint32_t, std::uint32_t>>;
    constexpr std::size_t kPlans = std::size_t{1} << 16;
    std::unordered_map<std::uint32_t, Plan> plans;
    std::queue<std::uint32_t> plan_order;  // oldest first, for eviction
    const auto keep_plan = [&](std::uint32_t id, Plan plan) {
      if (plans.size() >= kPlans) {
        plans.erase(plan_order.front());
        plan_order.pop();
      }
      if (plans.emplace(id, std::move(plan)).second) plan_order.push(id);
    };
    const auto bound = [&](const int* marking, std::vector<double>* dual_out, std::uint32_t id = std::numeric_limits<std::uint32_t>::max()) -> std::optional<std::uint64_t> {
      if (!marking_equation) return 0;
      std::vector<double> b(places, 0.0);
      for (std::size_t p = 0; p < places; ++p) b[p] = double(final_[p] - marking[p]);
      const auto lp = solve_lp_sparse(a, std::move(b), c, dual_out);
      if (lp.aborted) return 0;
      if (!lp.feasible) return std::nullopt;
      if (id != std::numeric_limits<std::uint32_t>::max() && std::abs(lp.objective - std::round(lp.objective)) < 1e-9) {
        Plan plan;
        bool integral = true;
        for (std::size_t t = 0; t < tcount && integral; ++t) {
          const double x = lp.solution[t];
          const double whole = std::round(x);
          integral = std::abs(x - whole) < 1e-9 && whole >= 0.0 && whole < 1e9;
          if (integral && whole >= 1.0)
            plan.emplace_back(static_cast<std::uint32_t>(t), static_cast<std::uint32_t>(whole));
        }
        if (integral) keep_plan(id, std::move(plan));
      }
      return static_cast<std::uint64_t>(std::max(0.0, std::ceil(lp.objective - 1e-6)));
    };
    // Kept duals: y, y·final and y·A_t per transition.
    struct Dual {
      std::vector<double> y, per_transition;
      double at_final = 0.0;
    };
    constexpr std::size_t kDualPool = 64;
    std::vector<Dual> pool;
    // Keeps the dual of the solve just made when it is feasible for every
    // transition (checked again here, to the solver's pivoting tolerance).
    const auto keep_dual = [&] {
      if (pool.size() >= kDualPool || dual.size() != places) return;
      Dual kept;
      kept.per_transition.assign(tcount, 0.0);
      for (std::size_t t = 0; t < tcount; ++t) {
        double yat = 0.0;
        for (const auto& [p, coef] : incidence_[t]) yat += dual[p] * coef;
        if (!std::isfinite(yat) || yat > c[t] + 1e-7) return;
        kept.per_transition[t] = yat;
      }
      for (std::size_t p = 0; p < places; ++p) {
        if (!std::isfinite(dual[p])) return;
        kept.at_final += dual[p] * double(final_[p]);
      }
      kept.y = dual;
      pool.push_back(std::move(kept));
    };
    // y·(final - m) for each kept dual at the marking being expanded.
    std::vector<double> at_current;
    std::vector<std::size_t> marked;
    std::vector<char> derived;
    Plan parent_plan;
    std::vector<std::size_t> fired;
    std::vector<int> successors;
    const auto dual_bound = [&](std::size_t t) -> std::uint64_t {
      double best = 0.0;
      for (std::size_t k = 0; k < at_current.size(); ++k)
        best = std::max(best, at_current[k] - pool[k].per_transition[t]);
      // Rounded down: at most the LP's rounded-up bound unless the dual's
      // error (1e-7 per firing, plus rounding) came near a whole unit.
      return static_cast<std::uint64_t>(std::floor(best + 1e-6));
    };
    constexpr std::uint64_t kUnseen = std::numeric_limits<std::uint64_t>::max();
    // The bound depends on the marking alone (solve_lp is deterministic), so
    // it is solved once per marking, including markings it rules out.
    struct Known {
      // kUnseen spelled out: MSVC rejects a local constant of the enclosing
      // function in a local class's default member initializer.
      std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
      bool bounded = false;
      std::optional<std::uint64_t> bound;
    };
    MarkingTable table(places);
    std::vector<Known> known;  // by marking id
    struct Entry {
      std::uint64_t f, g;
      std::uint32_t id;
      bool pending;  // f is g plus a lower bound; the marking's LP is not applied yet
    };
    // Orders like std::greater over (f, g, Marking).
    const auto later = [&](const Entry& left, const Entry& right) {
      if (left.f != right.f) return left.f > right.f;
      if (left.g != right.g) return left.g > right.g;
      return table.greater(left.id, right.id);
    };
    std::priority_queue<Entry, std::vector<Entry>, decltype(later)> frontier(later);
    const auto start = table.intern(initial_).first;
    known.push_back({0, true, bound(initial_.data(), nullptr)});
    const auto start_bound = known[start].bound;
    if (!start_bound) return std::nullopt;
    frontier.push({*start_bound, 0, start, false});
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap
                                                        : options.max_states;
    std::size_t expanded = 0;
    CancelPoll cancelled(options.cancelled);
    const auto out_of_time = [&] {
      return std::chrono::steady_clock::now() >= deadline || cancelled();
    };
    Marking current, successor;
    for (;;) {
      // Bring an exact entry to the top: the frontier the eager search would
      // hold is empty exactly when none is left.
      while (!frontier.empty() && frontier.top().pending) {
        if (out_of_time()) {
          exhausted = true;
          return std::nullopt;
        }
        auto entry = frontier.top();
        frontier.pop();
        auto& marking = known[entry.id];
        if (!marking.bounded) {
          marking.bound = bound(table.at(entry.id), &dual, entry.id);
          marking.bounded = true;
          // The kept duals fell short of this bound: keep this one too.
          if (marking.bound && *marking.bound > entry.f - entry.g) keep_dual();
        }
        if (!marking.bound) continue;
        entry.f = entry.g + *marking.bound;
        entry.pending = false;
        frontier.push(entry);
      }
      if (frontier.empty() || expanded >= budget) break;
      if (out_of_time()) {
        exhausted = true;
        return std::nullopt;
      }
      ++expanded;
      const auto [f, cost, id, pending] = frontier.top();
      (void)f;
      (void)pending;
      frontier.pop();
      if (known[id].best != kUnseen && cost > known[id].best) continue;
      table.copy(id, current);
      if (current == final_) return cost;
      marked.clear();
      for (std::size_t p = 0; p < places; ++p)
        if (current[p] != 0) marked.push_back(p);
      // A copy: keeping the successors' plans may evict this one.
      const auto found_plan = plans.find(id);
      const bool planned = found_plan != plans.end();
      if (planned) parent_plan = found_plan->second;
      derived.assign(tcount, 0);
      if (planned) for (const auto& [t, count] : parent_plan) derived[t] = 1;
      at_current.assign(pool.size(), 0.0);
      for (std::size_t k = 0; k < pool.size(); ++k) {
        double ym = 0.0;
        for (const auto p : marked) ym += pool[k].y[p] * double(current[p]);
        at_current[k] = pool[k].at_final - ym;
      }
      // The successors in transition order, interned as one batch.
      fired.clear();
      successors.clear();
      for (std::size_t t = 0; t < tcount; ++t) {
        if (!enabled(current, t)) continue;
        successor = current;
        fire_petri_transition(net_.transitions()[t], successor);
        fired.push_back(t);
        successors.insert(successors.end(), successor.begin(), successor.end());
      }
      table.intern_batch(successors.data(), fired.size(), [&](std::size_t k, std::uint32_t next, bool added) {
        const std::size_t t = fired[k];
        const std::uint64_t next_cost = cost + (net_.transitions()[t].silent() ? 0 : 1);
        if (added) known.emplace_back();
        if (known[next].best != kUnseen && known[next].best <= next_cost) return;
        if (known[next].bounded) {
          // Solved before: an infeasible marking is never queued, as in the
          // eager search.
          const auto h = known[next].bound;
          if (!h) return;
          known[next].best = next_cost;
          frontier.push({next_cost + *h, next_cost, next, false});
        } else if (derived[t] && *known[id].bound >= std::uint64_t(c[t])) {
          known[next].bounded = true;
          known[next].bound = *known[id].bound - std::uint64_t(c[t]);
          known[next].best = next_cost;
          frontier.push({next_cost + *known[next].bound, next_cost, next, false});
          Plan rest;
          rest.reserve(parent_plan.size());
          for (const auto& [u, count] : parent_plan)
            if (u != t) rest.emplace_back(u, count);
            else if (count > 1) rest.emplace_back(u, count - 1);
          keep_plan(next, std::move(rest));
        } else {
          // Recording the cost of a marking that may still prove infeasible
          // changes nothing: such a marking is never queued with an exact key,
          // so the best cost only skips pending entries that would be dropped.
          known[next].best = next_cost;
          frontier.push({next_cost + dual_bound(t), next_cost, next, true});
        }
      });
    }
    exhausted = !frontier.empty();
    return std::nullopt;
  }

  const PetriNet& net_;
  std::unordered_map<std::string, std::size_t> name_to_activity_;
  Marking initial_;
  Marking final_;
  bool reachable_ = false;
  bool reachability_exhausted_ = false;
  std::uint64_t empty_run_cost_ = 0;
  // marking_equation_describes_firing(net_): the LP bounds are admissible.
  bool marking_equation_ = true;
  // Per-transition sparse LP column for the marking-equation heuristic:
  // produced minus consumed tokens per place.
  std::vector<std::vector<std::pair<std::size_t, double>>> incidence_;
  // Exact heuristic bounds keyed by marking + remaining activity counts —
  // the aggregated LP depends on nothing else, so a bound computed for one
  // search state serves every state (of any trace variant) that shares them.
  // nullopt marks a marking-equation-infeasible key: a provable dead end.
  // Bounded so a pathological log cannot grow it without limit; past the cap
  // the aligner just solves without remembering.
  mutable std::unordered_map<std::string, std::optional<std::uint64_t>>
      bound_cache_;
};

// Bounds the model-footprint exploration so a highly concurrent net cannot
// enumerate markings forever; a bounded block-structured net is covered well
// within this many marking expansions.
constexpr std::size_t kFootprintCap = 200000;

using Relation = std::set<std::pair<std::string, std::string>>;

// The log's directly-follows relation: every ordered pair of adjacent
// activities seen in some trace. The pairs are collected as activity numbers
// and named once each.
Relation log_footprint(const EventLog& log) {
  std::unordered_map<std::string_view, std::uint32_t> ids;
  std::vector<std::string_view> names;
  std::unordered_set<std::uint64_t> pairs;
  for (const auto& trace : log.traces) {
    std::uint32_t previous = 0;
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      const std::string_view activity = trace.events[i].activity;
      const auto [found, added] = ids.try_emplace(activity, static_cast<std::uint32_t>(names.size()));
      if (added) names.push_back(activity);
      if (i > 0) pairs.insert(std::uint64_t{previous} << 32 | found->second);
      previous = found->second;
    }
  }
  Relation relation;
  for (const std::uint64_t pair : pairs)
    relation.emplace(std::string(names[pair >> 32]), std::string(names[pair & 0xFFFFFFFFu]));
  return relation;
}

// The model's directly-follows relation: explore the net's reachable markings,
// and for every activity a enabled at a marking, record a->b for each activity
// b enabled right after a fires. `exhausted` is set when the marking budget runs
// out before the reachable space is covered.
// The model's directly-follows relation: a > b iff some reachable marking
// fires a and can then fire b after silent moves only. One bounded pass
// expands every reachable marking once over all transitions; the visible
// labels observable at each marking are then propagated backwards over the
// silent edges to a fixpoint, so every marking's silent closure is computed
// once for the whole graph rather than re-searched per query — which is what
// keeps concurrent nets (whose marking spaces are large) tractable.
// `cancelled` is polled every 256 markings in each pass (the exploration of
// up to kFootprintCap markings can take seconds on a concurrent net).
// This is the general path: packed_footprint::model_footprint below computes
// the same relation for the ordinary nets the miners produce.
Relation generic_model_footprint(const PetriNet& net, const Marking& initial,
                                 bool& exhausted, const std::function<bool()>& cancelled) {
  CancellationCheck check_cancelled(cancelled, "footprint conformance was cancelled", 256);
  const auto& names = net.activities();
  const std::size_t acts = names.size();
  const std::size_t words = (acts + 63) / 64;

  // Markings by id in the order they were met, each stored once in one flat
  // pool (no node or vector per marking).
  MarkingTable index(initial.size());
  const auto intern = [&](const Marking& m) { return index.intern(m); };
  Marking current;  // the marking being expanded or scanned, copied out of the pool
  std::vector<const PetriTransition*> fired;
  std::vector<int> successors;

  const auto enabled = [&](const Marking& m, const PetriTransition& t) {
    return petri_transition_enabled(t, m);
  };
  Marking successor;  // reused firing buffer
  const auto fire = [&](const Marking& m, const PetriTransition& t) -> const Marking& {
    successor = m;
    fire_petri_transition(t, successor);
    return successor;
  };

  // Bounded breadth-first exploration. Only the silent edges are stored (for
  // the label propagation); visible successors are re-fired in the final pass
  // so the usually far larger visible edge set never has to be kept.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> silent_edges;
  std::vector<char> was_expanded;
  std::queue<std::uint32_t> frontier;
  frontier.push(intern(initial).first);
  std::size_t expanded = 0;
  const auto marking_count = [&] { return index.size(); };
  while (!frontier.empty()) {
    if (expanded++ >= kFootprintCap) {
      exhausted = true;
      break;
    }
    check_cancelled();
    const std::uint32_t id = frontier.front();
    frontier.pop();
    if (was_expanded.size() < marking_count()) was_expanded.resize(marking_count(), 0);
    was_expanded[id] = 1;
    index.copy(id, current);
    // The successors in transition order, interned as one batch.
    fired.clear();
    successors.clear();
    for (const auto& transition : net.transitions()) {
      if (!enabled(current, transition)) continue;
      fire(current, transition);
      fired.push_back(&transition);
      successors.insert(successors.end(), successor.begin(), successor.end());
    }
    index.intern_batch(successors.data(), fired.size(), [&](std::size_t k, std::uint32_t next, bool inserted) {
      if (fired[k]->silent()) silent_edges.emplace_back(id, next);
      if (inserted) frontier.push(next);
    });
  }

  const std::size_t count = marking_count();
  was_expanded.resize(count, 0);

  // labels[m]: bitset of activities enabled at m after silent moves only.
  // Seed with each marking's directly enabled visible transitions, then close
  // backwards over silent edges (u -> v silently implies labels(v) ⊆
  // labels(u)) with a worklist until stable.
  std::vector<std::uint64_t> labels(count * words, 0);
  const auto label_or = [&](std::uint32_t dst, std::uint32_t src) {
    bool changed = false;
    for (std::size_t w = 0; w < words; ++w) {
      const std::uint64_t merged = labels[dst * words + w] | labels[src * words + w];
      changed |= merged != labels[dst * words + w];
      labels[dst * words + w] = merged;
    }
    return changed;
  };
  for (std::uint32_t id = 0; id < count; ++id) {
    check_cancelled();
    index.copy(id, current);
    for (const auto& transition : net.transitions()) {
      if (transition.silent() || !enabled(current, transition)) continue;
      labels[id * words + *transition.activity / 64] |=
          std::uint64_t{1} << (*transition.activity % 64);
    }
  }
  std::vector<std::vector<std::uint32_t>> silent_preds(count);
  for (const auto& [from, to] : silent_edges) silent_preds[to].push_back(from);
  std::queue<std::uint32_t> work;
  std::vector<char> queued(count, 1);
  for (std::uint32_t id = 0; id < count; ++id) work.push(id);
  while (!work.empty()) {
    check_cancelled();
    const std::uint32_t id = work.front();
    work.pop();
    queued[id] = 0;
    for (const std::uint32_t pred : silent_preds[id]) {
      if (label_or(pred, id) && !queued[pred]) {
        queued[pred] = 1;
        work.push(pred);
      }
    }
  }

  // Emit the relation: for every visible edge m -a-> m', a is directly
  // followed by everything observable at m'. The pairs are first OR-ed into
  // one bit row per activity (a set of pairs ignores repeats), then named
  // once, rather than inserting a string pair for every edge and label.
  std::vector<std::uint64_t> follows(acts * words, 0);
  for (std::uint32_t id = 0; id < count; ++id) {
    check_cancelled();
    if (!was_expanded[id]) continue;
    index.copy(id, current);
    for (const auto& transition : net.transitions()) {
      if (transition.silent() || !enabled(current, transition)) continue;
      const auto found = index.find(fire(current, transition));
      if (!found) continue;
      const std::uint32_t next = *found;
      const std::size_t row = *transition.activity * words;
      for (std::size_t w = 0; w < words; ++w)
        follows[row + w] |= labels[next * words + w];
    }
  }
  Relation relation;
  for (std::size_t a = 0; a < acts; ++a)
    for (std::size_t b = 0; b < acts; ++b)
      if (follows[a * words + b / 64] & (std::uint64_t{1} << (b % 64)))
        relation.emplace(names[a], names[b]);
  return relation;
}

// The model footprint over packed markings, for nets whose ordinary arcs all
// have unit weight (repeated arcs allowed) and that have no reset arcs —
// every net the miners produce. It meets the markings in the same
// breadth-first order, gives them the same ids and stops at the same budget
// as generic_model_footprint, so the relation, `exhausted` and any overflow
// error are the same; what differs is the cost:
//  * a marking is a row of fixed-width token fields packed into 64-bit words
//    (one bit per place while the net stays safe, widened as soon as a place
//    holds more), so hashing, comparing and storing it touches a few words
//    instead of one int per place;
//  * the visible labels enabled at a marking are computed once, when it is
//    met, and the explored edges are kept (the silent ones as edges, the
//    visible ones as the labels entering each marking: one label row per
//    marking, as the general path keeps), so nothing is re-fired afterwards;
//  * only the transitions listed under a marked input place are tested at a
//    marking (still in transition order);
//  * the relation only grows as markings are expanded, and it can never hold
//    more than every pair of the live visible activities. Once more than
//    kFootprintCap markings are known — so the budget will be hit — and the
//    relation of what is expanded so far already holds all of those pairs
//    (checked for the pairs seen directly, a fires and then b is enabled at
//    once, after every expansion, and for the full relation at doubling
//    checkpoints), the result is fixed: exploring on cannot change it. That
//    stop is only taken when no firing within the budget can overflow a
//    token count, so it hides no error either.
namespace packed_footprint {

constexpr std::uint32_t kSilent = std::numeric_limits<std::uint32_t>::max();

struct Arc {
  std::uint32_t place = 0;
  std::uint32_t count = 0;  // repeated arcs between the place and the transition
  std::uint32_t word = 0;   // the place's field in the current layout
  std::uint32_t shift = 0;
};

// A transition's arcs are arcs[in, out) inputs, [out, inhibit) outputs and
// [inhibit, end) inhibitors, all transitions' arcs in one array.
struct Step {
  bool live = true;               // every input and inhibitor place exists
  std::uint32_t label = kSilent;  // the activity, or kSilent
  std::uint32_t in = 0, out = 0, inhibit = 0, end = 0;
};

// Rows of `words` 64-bit words, interned under consecutive ids. Only the slot
// layout depends on the hash.
class Table {
 public:
  explicit Table(std::size_t words) : words_(words), slots_(1024, 0) {}
  Table(const Table&) = delete;
  Table& operator=(const Table&) = delete;

  [[nodiscard]] std::size_t size() const { return count_; }
  [[nodiscard]] const std::uint64_t* at(std::uint32_t id) const {
    return blocks_[id / kRows].get() + std::size_t(id % kRows) * words_;
  }
  std::pair<std::uint32_t, bool> intern(const std::uint64_t* row) {
    if ((count_ + 1) * 2 > slots_.size()) grow();
    return intern(row, row_hash(row));
  }
  // Interns `count` rows stored back to back, in order, calling
  // visit(k, id, added) for the k-th: the ids of interning them one at a
  // time. The slots, and then the rows they point to, are read in passes of
  // independent loads first, so the cache misses of a large table overlap
  // instead of being taken one after another.
  template <class Visit>
  void intern_batch(const std::uint64_t* rows, std::size_t count, Visit&& visit) {
    while ((count_ + count) * 2 > slots_.size()) grow();
    const std::size_t mask = slots_.size() - 1;
    hashes_.resize(count);
    std::uint64_t touched = 0;
    for (std::size_t k = 0; k < count; ++k) {
      hashes_[k] = row_hash(rows + k * words_);
      touched += slots_[hashes_[k] & mask];
    }
    for (std::size_t k = 0; k < count; ++k) {
      const std::uint64_t entry = slots_[hashes_[k] & mask];
      if (entry != 0 && (entry & kTagMask) == (hashes_[k] & kTagMask))
        touched += *at(static_cast<std::uint32_t>((entry & ~kTagMask) - 1));
    }
    touch_ = touch_ + touched;
    for (std::size_t k = 0; k < count; ++k) {
      const auto [id, added] = intern(rows + k * words_, hashes_[k]);
      visit(k, id, added);
    }
  }
  // Rewrites every row as recode(old_row, new_row) with `words` words each;
  // the ids stay.
  template <class Recode>
  void relayout(std::size_t words, Recode&& recode) {
    std::vector<std::unique_ptr<std::uint64_t[]>> blocks;
    for (std::size_t id = 0; id < count_; ++id) {
      if (id % kRows == 0)
        blocks.push_back(std::make_unique_for_overwrite<std::uint64_t[]>(kRows * words));
      recode(at(static_cast<std::uint32_t>(id)), blocks.back().get() + (id % kRows) * words);
      if (id % kRows == kRows - 1) blocks_[id / kRows].reset();
    }
    blocks_ = std::move(blocks);
    words_ = words;
    rehash(slots_.size());
  }

 private:
  static constexpr std::size_t kRows = 4096;
  static constexpr std::uint64_t kTagMask = 0xFFFFFFFF00000000ull;

  // The table has room for the row.
  std::pair<std::uint32_t, bool> intern(const std::uint64_t* row, std::uint64_t hash) {
    const std::uint64_t tag = hash & kTagMask;
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = hash & mask;
    for (; slots_[slot] != 0; slot = (slot + 1) & mask) {
      const std::uint64_t entry = slots_[slot];
      if ((entry & kTagMask) != tag) continue;
      const auto id = static_cast<std::uint32_t>((entry & ~kTagMask) - 1);
      if (std::equal(row, row + words_, at(id))) return {id, false};
    }
    if (count_ >= std::numeric_limits<std::uint32_t>::max() - 1)
      throw std::length_error("model-run search marking table overflow");
    const auto id = static_cast<std::uint32_t>(count_++);
    if (id % kRows == 0)
      blocks_.push_back(std::make_unique_for_overwrite<std::uint64_t[]>(kRows * words_));
    std::copy(row, row + words_, blocks_.back().get() + std::size_t(id % kRows) * words_);
    slots_[slot] = tag | (std::uint64_t{id} + 1);
    return {id, true};
  }
  [[nodiscard]] std::uint64_t row_hash(const std::uint64_t* row) const {
    std::uint64_t h = 0x9E3779B97F4A7C15ull;
    for (std::size_t w = 0; w < words_; ++w) {
      h ^= row[w];
      h *= 0xBF58476D1CE4E5B9ull;
      h ^= h >> 31;
    }
    h *= 0x94D049BB133111EBull;
    return h ^ (h >> 29);
  }
  void grow() { rehash(slots_.size() * 2); }
  void rehash(std::size_t size) {
    slots_.assign(size, 0);
    const std::size_t mask = size - 1;
    for (std::size_t id = 0; id < count_; ++id) {
      const std::uint64_t hash = row_hash(at(static_cast<std::uint32_t>(id)));
      std::size_t slot = hash & mask;
      while (slots_[slot] != 0) slot = (slot + 1) & mask;
      slots_[slot] = (hash & kTagMask) | (std::uint64_t{id} + 1);
    }
  }

  std::size_t words_;
  std::size_t count_ = 0;
  std::vector<std::unique_ptr<std::uint64_t[]>> blocks_;
  std::vector<std::uint64_t> slots_;  // 0, or the hash's high half | id + 1
  std::vector<std::uint64_t> hashes_;  // of the batch being interned
  // Sums what the batch passes read; volatile, so the reads are not dropped.
  volatile std::uint64_t touch_ = 0;
};

// Whether packed_model_footprint applies: unit ordinary arcs, no reset arcs,
// every output place present and every activity index in range (otherwise the
// general path decides).
bool applies(const PetriNet& net, const Marking& initial) {
  const std::size_t places = initial.size();
  if (places >= std::numeric_limits<std::uint32_t>::max()) return false;
  for (const auto& transition : net.transitions()) {
    if (!transition.input_weights.empty() || !transition.output_weights.empty() ||
        !transition.reset_inputs.empty())
      return false;
    for (const auto place : transition.outputs)
      if (place >= places) return false;
    if (transition.activity && *transition.activity >= net.activities().size()) return false;
  }
  return true;
}

Relation model_footprint(const PetriNet& net, const Marking& initial, bool& exhausted,
                         const std::function<bool()>& cancelled) {
  CancellationCheck check_cancelled(cancelled, "footprint conformance was cancelled", 256);
  const auto& names = net.activities();
  const std::size_t acts = names.size();
  const std::size_t lw = (acts + 63) / 64;  // label words per marking
  const std::size_t places = initial.size();

  // The transitions as place/count arcs, in transition order.
  std::vector<Step> steps;
  steps.reserve(net.transitions().size());
  std::vector<std::uint32_t> visible;  // indices of the live visible steps
  std::vector<char> live_activity(acts, 0);
  std::uint64_t most_produced = 0;  // tokens one firing adds, at most
  std::vector<Arc> arcs;
  const auto gather = [&](const std::vector<std::size_t>& list) {
    const std::size_t first = arcs.size();
    for (const auto place : list) {
      const auto found = std::find_if(arcs.begin() + static_cast<std::ptrdiff_t>(first), arcs.end(),
                                      [&](const Arc& arc) { return arc.place == place; });
      if (found != arcs.end()) ++found->count;
      else arcs.push_back({static_cast<std::uint32_t>(place), 1, 0, 0});
    }
    if (arcs.size() >= std::numeric_limits<std::uint32_t>::max())
      throw std::length_error("model footprint arc table overflow");
    return static_cast<std::uint32_t>(arcs.size());
  };
  for (const auto& transition : net.transitions()) {
    Step step;
    for (const auto place : transition.inputs) step.live = step.live && place < places;
    for (const auto place : transition.inhibitor_inputs) step.live = step.live && place < places;
    step.in = static_cast<std::uint32_t>(arcs.size());
    step.out = step.live ? gather(transition.inputs) : step.in;
    step.inhibit = gather(transition.outputs);
    step.end = step.live ? gather(transition.inhibitor_inputs) : step.inhibit;
    most_produced = std::max<std::uint64_t>(most_produced, transition.outputs.size());
    if (!transition.silent()) {
      step.label = static_cast<std::uint32_t>(*transition.activity);
      if (step.live) {
        visible.push_back(static_cast<std::uint32_t>(steps.size()));
        live_activity[step.label] = 1;
      }
    }
    steps.push_back(std::move(step));
  }
  std::uint64_t full_pairs = 0;  // every pair of live visible activities
  for (const char live : live_activity) full_pairs += live != 0;
  full_pairs *= full_pairs;
  // No marking within the budget can reach this many tokens in a place (each
  // is at most kFootprintCap firings from the initial one), so no firing the
  // general path would make can overflow, and stopping early hides no error.
  std::uint64_t most_initial = 0;
  for (const int tokens : initial) most_initial = std::max<std::uint64_t>(most_initial, std::uint64_t(tokens));
  const std::uint64_t int_max = std::uint64_t(std::numeric_limits<int>::max());
  const bool may_stop_early =
      most_initial <= int_max &&
      most_produced <= (int_max - most_initial) / (std::uint64_t(kFootprintCap) + 1);

  // The packed layout: `bits` per place, fields never straddle a word.
  unsigned bits = 1;
  while (bits < 32 && (std::uint64_t{1} << bits) - 1 < most_initial) bits *= 2;
  std::uint64_t field_max = 0;
  std::size_t words = 1;
  const auto layout = [&] {
    const std::size_t per_word = 64 / bits;
    field_max = (std::uint64_t{1} << bits) - 1;
    words = std::max<std::size_t>(1, (places + per_word - 1) / per_word);
    for (auto& arc : arcs) {
      arc.word = static_cast<std::uint32_t>(arc.place / per_word);
      arc.shift = static_cast<std::uint32_t>((arc.place % per_word) * bits);
    }
  };
  layout();
  const auto field = [&](const std::uint64_t* row, const Arc& arc) {
    return (row[arc.word] >> arc.shift) & field_max;
  };
  const auto enabled = [&](const std::uint64_t* row, const Step& step) {
    if (!step.live) return false;
    for (std::uint32_t a = step.in; a < step.out; ++a)
      if (field(row, arcs[a]) < arcs[a].count) return false;
    for (std::uint32_t a = step.inhibit; a < step.end; ++a)
      if (field(row, arcs[a]) != 0) return false;
    return true;
  };
  // Fires an enabled step on `row` in place. Returns 0, or the token count a
  // place would need when it does not fit the layout (the row is then
  // unusable); a count past int's range is the general path's overflow.
  const auto fire = [&](std::uint64_t* row, const Step& step) -> std::uint64_t {
    for (std::uint32_t a = step.in; a < step.out; ++a)
      row[arcs[a].word] -= std::uint64_t{arcs[a].count} << arcs[a].shift;
    for (std::uint32_t a = step.out; a < step.inhibit; ++a) {
      const Arc& arc = arcs[a];
      const std::uint64_t tokens = field(row, arc) + arc.count;
      if (tokens > int_max) throw std::overflow_error("Petri firing exceeds supported token range");
      if (tokens > field_max) return tokens;
      row[arc.word] += std::uint64_t{arc.count} << arc.shift;
    }
    return 0;
  };

  // Each live step is listed under its first input place (or as always
  // possible when it has none): only the steps listed under a marked place can
  // be enabled, so an expansion tests those, still in transition order.
  std::vector<std::uint32_t> listed_begin(places + 1, 0), listed, unconditional;
  for (const auto& step : steps)
    if (step.live && step.in != step.out) ++listed_begin[arcs[step.in].place + 1];
  for (std::size_t place = 0; place < places; ++place) listed_begin[place + 1] += listed_begin[place];
  listed.resize(listed_begin[places]);
  {
    std::vector<std::uint32_t> fill(listed_begin.begin(), listed_begin.end() - 1);
    for (std::size_t index = 0; index < steps.size(); ++index) {
      const Step& step = steps[index];
      if (!step.live) continue;
      if (step.in == step.out) unconditional.push_back(static_cast<std::uint32_t>(index));
      else listed[fill[arcs[step.in].place]++] = static_cast<std::uint32_t>(index);
    }
  }
  std::vector<std::uint64_t> candidates((steps.size() + 63) / 64, 0);
  const auto find_candidates = [&](const std::uint64_t* row) {
    std::fill(candidates.begin(), candidates.end(), 0);
    for (const auto index : unconditional) candidates[index / 64] |= std::uint64_t{1} << (index % 64);
    const std::size_t per_word = 64 / bits;
    for (std::size_t w = 0; w < words; ++w) {
      for (std::uint64_t rest = row[w]; rest != 0;) {
        const auto field_index = static_cast<std::size_t>(std::countr_zero(rest)) / bits;
        rest &= ~(field_max << (field_index * bits));
        const std::size_t place = w * per_word + field_index;
        for (std::uint32_t k = listed_begin[place]; k < listed_begin[place + 1]; ++k)
          candidates[listed[k] / 64] |= std::uint64_t{1} << (listed[k] % 64);
      }
    }
  };

  std::vector<std::uint64_t> packed(words, 0);
  {
    const std::size_t per_word = 64 / bits;
    for (std::size_t place = 0; place < places; ++place)
      packed[place / per_word] |= std::uint64_t(initial[place]) << ((place % per_word) * bits);
  }
  Table table(words);
  // seeds[m]: the activities whose visible transitions are enabled at m.
  // entered[m]: the activities fired by an expanded marking into m.
  std::vector<std::uint64_t> seeds, entered;
  const auto seed_new = [&](const std::uint64_t* row) {  // the marking met last
    seeds.resize(seeds.size() + lw, 0);
    entered.resize(entered.size() + lw, 0);
    std::uint64_t* seed = seeds.data() + seeds.size() - lw;
    for (const auto index : visible) {
      const Step& step = steps[index];
      if (enabled(row, step)) seed[step.label / 64] |= std::uint64_t{1} << (step.label % 64);
    }
  };
  table.intern(packed.data());
  seed_new(packed.data());

  const auto widen = [&](std::uint64_t needed) {
    const unsigned old_bits = bits;
    const std::uint64_t old_max = field_max;
    while ((std::uint64_t{1} << bits) - 1 < needed) bits *= 2;
    const std::size_t old_per_word = 64 / old_bits;
    layout();
    const std::size_t per_word = 64 / bits;
    table.relayout(words, [&](const std::uint64_t* from, std::uint64_t* to) {
      std::fill(to, to + words, 0);
      for (std::size_t place = 0; place < places; ++place) {
        const std::uint64_t tokens =
            (from[place / old_per_word] >> ((place % old_per_word) * old_bits)) & old_max;
        to[place / per_word] |= tokens << ((place % per_word) * bits);
      }
    });
  };

  // Breadth-first: the frontier is every id from `next` on. Only the silent
  // edges of each expanded marking are kept (their targets, in firing order);
  // a visible edge m -a-> m' is kept as a in entered[m'], which is all the
  // relation needs of it. Memory therefore stays one label row per marking
  // whatever the number of visible transitions enabled at each marking.
  std::vector<std::uint32_t> silent_target;
  std::vector<std::size_t> silent_begin{0};

  // The relation once the first `expanded` markings are expanded, as one bit
  // row per activity: a > b for every visible edge m -a-> m' and every b
  // observable at m' after silent moves only. Observable labels are the
  // least fixpoint of the seeds closed backwards over the silent edges; a
  // marking that was not expanded has no recorded edges, so its labels are
  // its seeds. `last` closes the seeds in place (they are not needed after).
  const auto relation_rows = [&](std::size_t expanded, bool last) {
    std::vector<std::uint64_t> labels =
        last ? std::move(seeds)
             : std::vector<std::uint64_t>(seeds.begin(),
                                          seeds.begin() + static_cast<std::ptrdiff_t>(expanded * lw));
    // The rows of the markings that were not expanded: their seeds.
    const std::uint64_t* outside = last ? labels.data() : seeds.data();
    std::vector<std::size_t> pred_begin(expanded + 1, 0);
    for (std::size_t m = 0; m < expanded; ++m) {
      check_cancelled();
      for (std::size_t e = silent_begin[m]; e < silent_begin[m + 1]; ++e) {
        const std::uint32_t target = silent_target[e];
        if (target < expanded) {
          ++pred_begin[target + 1];
          continue;
        }
        for (std::size_t w = 0; w < lw; ++w)
          labels[m * lw + w] |= outside[std::size_t(target) * lw + w];
      }
    }
    for (std::size_t m = 0; m < expanded; ++m) pred_begin[m + 1] += pred_begin[m];
    std::vector<std::uint32_t> preds(pred_begin[expanded]);
    {
      std::vector<std::size_t> fill(pred_begin.begin(), pred_begin.end() - 1);
      for (std::size_t m = 0; m < expanded; ++m)
        for (std::size_t e = silent_begin[m]; e < silent_begin[m + 1]; ++e)
          if (silent_target[e] < expanded)
            preds[fill[silent_target[e]]++] = static_cast<std::uint32_t>(m);
    }
    std::vector<std::uint32_t> work(expanded);
    for (std::size_t m = 0; m < expanded; ++m)
      work[m] = static_cast<std::uint32_t>(expanded - 1 - m);
    std::vector<char> queued(expanded, 1);
    while (!work.empty()) {
      check_cancelled();
      const std::uint32_t id = work.back();
      work.pop_back();
      queued[id] = 0;
      const std::uint64_t* from = labels.data() + std::size_t(id) * lw;
      for (std::size_t k = pred_begin[id]; k < pred_begin[id + 1]; ++k) {
        const std::uint32_t pred = preds[k];
        std::uint64_t* into = labels.data() + std::size_t(pred) * lw;
        bool changed = false;
        for (std::size_t w = 0; w < lw; ++w) {
          changed |= (from[w] & ~into[w]) != 0;
          into[w] |= from[w];
        }
        if (changed && !queued[pred]) {
          queued[pred] = 1;
          work.push_back(pred);
        }
      }
    }
    std::vector<std::uint64_t> follows(acts * lw, 0);
    const std::size_t met = lw == 0 ? 0 : entered.size() / lw;
    for (std::size_t m = 0; m < met; ++m) {
      check_cancelled();
      const std::uint64_t* into = entered.data() + m * lw;
      const std::uint64_t* seen = m < expanded ? labels.data() + m * lw : outside + m * lw;
      for (std::size_t aw = 0; aw < lw; ++aw) {
        for (std::uint64_t rest = into[aw]; rest != 0; rest &= rest - 1) {
          const std::size_t label = aw * 64 + static_cast<std::size_t>(std::countr_zero(rest));
          for (std::size_t w = 0; w < lw; ++w) follows[label * lw + w] |= seen[w];
        }
      }
    }
    return follows;
  };
  const auto pairs_in = [](const std::vector<std::uint64_t>& rows) {
    std::uint64_t pairs = 0;
    for (const std::uint64_t word : rows) pairs += std::uint64_t(std::popcount(word));
    return pairs;
  };
  const auto named = [&](const std::vector<std::uint64_t>& rows) {
    Relation relation;
    for (std::size_t a = 0; a < acts; ++a)
      for (std::size_t b = 0; b < acts; ++b)
        if (rows[a * lw + b / 64] & (std::uint64_t{1} << (b % 64)))
          relation.emplace(names[a], names[b]);
    return relation;
  };

  // `direct` (a fires, then b is enabled at once) is a lower bound kept as the
  // edges are met; the closed relation is checked as well at checkpoints
  // (doubling, so they cost a bounded share of the final closure) once the
  // budget is known to be hit.
  std::vector<std::uint64_t> direct(acts * lw, 0);
  std::uint64_t direct_pairs = 0;
  std::size_t checkpoint = 0;
  std::vector<std::uint64_t> current, successors;
  std::vector<std::uint32_t> labels_fired;
  std::size_t next = 0;
  while (next < table.size()) {
    if (next >= kFootprintCap) {
      exhausted = true;
      break;
    }
    if (may_stop_early && table.size() > kFootprintCap) {
      if (direct_pairs == full_pairs) {
        exhausted = true;
        return named(direct);
      }
      if (next >= checkpoint) {
        auto rows = relation_rows(next, false);
        if (pairs_in(rows) == full_pairs) {
          exhausted = true;
          return named(rows);
        }
        checkpoint = std::max<std::size_t>(2 * next, 1024);
      }
    }
    check_cancelled();
    for (bool fits = false; !fits;) {
      current.assign(table.at(static_cast<std::uint32_t>(next)),
                     table.at(static_cast<std::uint32_t>(next)) + words);
      successors.clear();
      labels_fired.clear();
      fits = true;
      find_candidates(current.data());
      for (std::size_t cw = 0; fits && cw < candidates.size(); ++cw) {
        for (std::uint64_t rest = candidates[cw]; rest != 0; rest &= rest - 1) {
          const Step& step = steps[cw * 64 + static_cast<std::size_t>(std::countr_zero(rest))];
          if (!enabled(current.data(), step)) continue;
          const std::size_t base = successors.size();
          successors.insert(successors.end(), current.begin(), current.end());
          if (const auto needed = fire(successors.data() + base, step)) {
            widen(needed);
            fits = false;
            break;
          }
          labels_fired.push_back(step.label);
        }
      }
    }
    table.intern_batch(successors.data(), labels_fired.size(),
                       [&](std::size_t k, std::uint32_t target, bool added) {
      if (added) seed_new(successors.data() + k * words);
      const std::uint32_t label = labels_fired[k];
      if (label == kSilent) {
        silent_target.push_back(target);
        return;
      }
      entered[std::size_t(target) * lw + label / 64] |= std::uint64_t{1} << (label % 64);
      const std::uint64_t* seed = seeds.data() + std::size_t(target) * lw;
      std::uint64_t* row = direct.data() + std::size_t(label) * lw;
      for (std::size_t w = 0; w < lw; ++w) {
        const std::uint64_t fresh = seed[w] & ~row[w];
        if (fresh == 0) continue;
        direct_pairs += std::uint64_t(std::popcount(fresh));
        row[w] |= fresh;
      }
    });
    silent_begin.push_back(silent_target.size());
    ++next;
  }
  return named(relation_rows(next, true));
}

}  // namespace packed_footprint

Relation model_footprint(const PetriNet& net, const Marking& initial,
                         bool& exhausted, const std::function<bool()>& cancelled) {
  if (packed_footprint::applies(net, initial))
    return packed_footprint::model_footprint(net, initial, exhausted, cancelled);
  return generic_model_footprint(net, initial, exhausted, cancelled);
}

FootprintResult compare_footprints(const Relation& log, const Relation& model,
                                   bool exhausted) {
  FootprintResult result;
  result.exhausted = exhausted;
  result.log_relations = log.size();
  result.model_relations = model.size();
  for (const auto& pair : log) {
    if (model.count(pair)) {
      ++result.shared_relations;
    } else {
      result.log_only.push_back({pair.first, pair.second});
    }
  }
  for (const auto& pair : model) {
    if (!log.count(pair)) result.model_only.push_back({pair.first, pair.second});
  }
  result.fitness = log.empty()
                       ? 1.0
                       : double(result.shared_relations) / double(log.size());
  result.precision =
      model.empty() ? 1.0
                    : double(result.shared_relations) / double(model.size());
  return result;
}

}  // namespace

TokenReplayKeyError::TokenReplayKeyError(std::string key)
    : std::invalid_argument(replay_key_repr(key)),
      reference_args_{std::move(key)} {}

FootprintResult check_footprints(const EventLog& event_log,
                                 const PetriNet& net) {
  return check_footprints(event_log, net, {});
}

FootprintResult check_footprints(const EventLog& event_log, const PetriNet& net,
                                 const std::vector<int>& initial_marking) {
  return check_footprints(event_log, net, initial_marking, {});
}

FootprintResult check_footprints(const EventLog& event_log, const PetriNet& net,
                                 const std::vector<int>& initial_marking,
                                 const std::function<bool()>& cancelled) {
  const auto initial = resolve_workflow_marking(net, initial_marking, true);
  bool exhausted = false;
  const auto model = model_footprint(net, initial, exhausted, cancelled);
  return compare_footprints(log_footprint(event_log), model, exhausted);
}

FootprintResult check_footprints(const EventLog& event_log,
                                 const ProcessTree& tree) {
  return check_footprints(event_log, to_petri_net(tree));
}

namespace {
thread_local std::size_t g_enabled_search_budget = 0;
}  // namespace

std::size_t detail::enabled_search_budget() noexcept {
  return g_enabled_search_budget == 0 ? kEnabledSearchBudget : g_enabled_search_budget;
}

detail::ScopedEnabledSearchBudget::ScopedEnabledSearchBudget(std::size_t budget) noexcept
    : previous_(g_enabled_search_budget) {
  g_enabled_search_budget = budget;
}

detail::ScopedEnabledSearchBudget::~ScopedEnabledSearchBudget() {
  g_enabled_search_budget = previous_;
}

std::vector<std::size_t> detail::initial_enabled_token_activities(
    const PetriNet& net, const TokenReplayOptions& options) {
  // The reference scores the empty prefix with the same eventually-enabled
  // worklist as every other prefix (transition-name order, newest marking per
  // queued transition, one visit per transition and marking), whichever replay
  // variant scores the remaining prefixes.
  auto forward = options;
  forward.variant = TokenReplayVariant::kForward;
  const Replayer replayer(net, forward.initial_marking, forward.final_marking, &forward);
  std::set<std::size_t> activities;
  for (const auto transition : replayer.enabled_transition_ids(
           replayer.initial_marking(), forward, detail::enabled_search_budget()))
    if (const auto& activity = net.transitions()[transition].activity) activities.insert(*activity);
  return {activities.begin(), activities.end()};
}

ConformanceResult check_conformance(const EventLog& event_log,
                                    const PetriNet& net,
                                    TokenReplayOptions options) {
  validate_token_replay_event_limit(event_log, options);
  if (!valid_token_replay_variant(options.variant)) {
    throw std::invalid_argument("unsupported token replay variant");
  }
  const Replayer replayer(net, options.initial_marking, options.final_marking, &options);
  const auto& activity_attribute = options.activity_attribute;
  const auto variants = collect_replay_variants(event_log, replayer,
                                          activity_attribute);

  ConformanceResult result;
  result.trace_count = event_log.trace_count();
  result.fitness = 1.0;
  result.precision = 1.0;
  result.activity_attribute = activity_attribute;

  std::vector<std::uint64_t> executions(net.transitions().size(), 0);
  std::vector<std::uint64_t> variant_executions;
  CancellationCheck cancelled(options.cancelled, "token replay was cancelled", 16);
  for (const auto& variant : variants) {
    cancelled();
    variant_executions.assign(net.transitions().size(), 0);
    const bool backwards = options.variant == TokenReplayVariant::kBackwards;
    const auto backwards_outcome =
        backwards ? std::optional<Replayer::BackwardsOutcome>(
                        replayer.replay_backwards(variant.ids, &variant_executions,
                                                  options.stop_immediately_unfit,
                                                  options.walk_through_hidden_transitions))
                  : std::nullopt;
    const auto counts = backwards
                            ? backwards_outcome->counts
                            : replayer.replay(variant.ids, &variant_executions,
                                              options);
    for (std::size_t t = 0; t < executions.size(); ++t) {
      executions[t] += variant_executions[t] * variant.count;
    }
    result.produced += counts.produced * variant.count;
    result.consumed += counts.consumed * variant.count;
    result.missing += counts.missing * variant.count;
    result.remaining += counts.remaining * variant.count;
    const bool fitting = backwards
                             ? backwards_outcome->fitting
                             : counts.forward_fitting(options);
    if (fitting) {
      result.fitting_traces += variant.count;
    }
    result.variants.push_back(
        VariantConformance{variant.names, variant.count, counts.fitness(),
                           variant.null_activity_indices});
  }

  if (result.consumed > 0 || result.produced > 0) {
    const double consume_term =
        result.consumed == 0 ? 0.0 : double(result.missing) / double(result.consumed);
    const double produce_term =
        result.produced == 0 ? 0.0 : double(result.remaining) / double(result.produced);
    result.fitness = 0.5 * (1.0 - consume_term) + 0.5 * (1.0 - produce_term);
  }

  // ETConformance precision is the evaluator's computation: it replays each
  // distinct proper prefix with stop-immediately-unfit and no final routing,
  // whatever the fitness replay was asked to do, and scores the labels the
  // reference's eventually-enabled worklist reports at the reached marking.
  try {
    std::vector<detail::TokenPrecisionSequence> sequences;
    sequences.reserve(variants.size());
    for (const auto& variant : variants) {
      auto& sequence = sequences.emplace_back();
      sequence.count = variant.count;
      for (std::size_t step = 0; step < variant.labels.size(); ++step)
        sequence.steps.push_back({variant.labels[step], variant.identities[step]});
    }
    result.precision = detail::token_etconformance_precision(sequences, net, options);
  } catch (const OperationCancelled&) {
    throw;
  } catch (const std::runtime_error&) {
    // Silent-search budgets (the enabled-label worklist and the routing
    // searches) refuse to produce a partial value.
    result.precision = std::numeric_limits<double>::quiet_NaN();
    result.precision_computed = false;
  } catch (const std::domain_error&) {
    // A backwards prefix with no consumed or produced tokens has no fitness;
    // the reference evaluator raises there, so no precision value exists.
    result.precision = std::numeric_limits<double>::quiet_NaN();
    result.precision_computed = false;
  }
  result.generalization = compute_generalization(executions);
  result.transition_frequencies = executions;
  result.simplicity = compute_simplicity(net);
  if (options.include_trace_diagnostics || options.include_duration_diagnostics ||
      options.include_root_cause_diagnostics) {
    auto traces = replay_token_traces(event_log, net, options);
    if (options.include_duration_diagnostics)
      result.duration_diagnostics = compute_token_replay_duration_diagnostics(
          event_log, net, traces, options);
    if (options.include_root_cause_diagnostics)
      result.root_cause_diagnostics = compute_token_replay_root_cause_diagnostics(
          event_log, net, traces, options);
    if (options.include_trace_diagnostics)
      result.trace_diagnostics = std::move(traces);
  }

  // Surface the least-fitting variants first; break ties by frequency so the
  // most impactful deviations lead.
  std::stable_sort(result.variants.begin(), result.variants.end(),
                   [](const VariantConformance& a, const VariantConformance& b) {
                     if (a.fitness != b.fitness) return a.fitness < b.fitness;
                     return a.count > b.count;
                   });
  return result;
}

ConformanceResult check_conformance(const EventLog& event_log,
                                    const ProcessTree& tree,
                                    TokenReplayOptions options) {
  validate_token_replay_event_limit(event_log, options);
  return check_conformance(event_log, to_petri_net(tree), options);
}

TokenReplayDiagnostics replay_token_diagnostics(
    const EventLog& event_log, const PetriNet& net,
    TokenReplayOptions options) {
  validate_token_replay_event_limit(event_log, options);
  if (!valid_token_replay_variant(options.variant)) {
    throw std::invalid_argument("unsupported token replay variant");
  }
  const Replayer replayer(net, options.initial_marking, options.final_marking, &options);
  const auto& activity_attribute = options.activity_attribute;
  TokenReplayDiagnostics result;
  result.trace_count = event_log.trace_count();
  result.places.resize(replayer.initial_marking().size());
  result.marking_domain = replayer.marking_domain();
  result.transition_activations.resize(net.transitions().size());
  for (const auto& trace : event_log.traces) {
    std::vector<std::optional<std::size_t>> ids;
    ids.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto activity = selected_replay_activity(event, activity_attribute);
      ids.push_back(activity.model_label
                        ? replayer.activity_id(*activity.model_label)
                        : std::nullopt);
    }

    std::vector<std::uint64_t> executions(net.transitions().size(), 0);
    const bool backwards = options.variant == TokenReplayVariant::kBackwards;
    const auto backwards_outcome =
        backwards
            ? std::optional<Replayer::BackwardsOutcome>(replayer.replay_backwards(
                  ids, &executions, options.stop_immediately_unfit,
                  options.walk_through_hidden_transitions))
            : std::nullopt;
    const auto counts = backwards
                            ? backwards_outcome->counts
                            : replayer.replay(ids, &executions, options);
    for (std::size_t place = 0; place < result.places.size(); ++place) {
      result.places[place].missing += counts.places[place].missing;
      result.places[place].remaining += counts.places[place].remaining;
      result.places[place].consumed += counts.places[place].consumed;
      result.places[place].produced += counts.places[place].produced;
    }
    for (std::size_t transition = 0;
         transition < result.transition_activations.size(); ++transition) {
      result.transition_activations[transition] += executions[transition];
    }
    const bool fitting = backwards
                             ? backwards_outcome->fitting
                             : counts.forward_fitting(options);
    if (fitting) ++result.fitting_traces;
  }
  return result;
}

namespace {

// The reference keeps one "activity not in model" flag for a whole forward
// replay: once a variant with such an activity was replayed, every variant
// replayed after it is unfit too. Groups are row indices in replay order.
void carry_unknown_activity_unfitness(
    std::vector<TokenReplayTraceDiagnostics>& rows,
    const std::vector<std::vector<std::size_t>>& groups) {
  bool seen = false;
  for (const auto& group : groups) {
    for (const auto row : group) seen |= !rows[row].unknown_event_indices.empty();
    if (seen) for (const auto row : group) rows[row].trace_is_fit = false;
  }
}

// `enabled_transitions` false leaves each row's enabled_transitions empty:
// computing them at the reached marking dominates the replay's cost, and
// callers that only read the firings (token_replay_firing_sequences) skip it.
std::vector<TokenReplayTraceDiagnostics> replay_token_trace_rows(
    const EventLog& event_log, const PetriNet& net, TokenReplayOptions options,
    bool calculate_fitness, bool full_result = false,
    bool enabled_transitions = true) {
  validate_token_replay_event_limit(event_log, options);
  if (!valid_token_replay_variant(options.variant))
    throw std::invalid_argument("unsupported token replay variant");
  const Replayer replayer(net, options.initial_marking, options.final_marking, &options, full_result);
  const auto& activity_attribute = options.activity_attribute;
  if (options.variant == TokenReplayVariant::kBackwards) {
    // Resolve a whole trace before validating its variant key, and finish
    // validating the log before any trace can fail its fitness calculation.
    for (const auto& trace : event_log.traces) {
      for (const auto& event : trace.events)
        (void)selected_replay_activity(event, activity_attribute);
      for (const auto& event : trace.events) {
        const auto cell = native_event_attribute(event, activity_attribute);
        if (cell && !root_cause_key_is_hashable(*cell)) {
          // Retained XES detail does not change a scalar string's replay label.
          const auto activity = selected_replay_activity(event, activity_attribute);
          if (!activity.model_label && activity.display_name)
            throw std::invalid_argument("unhashable activity variant key");
        }
      }
    }
  }
  std::vector<TokenReplayTraceDiagnostics> result;
  result.reserve(event_log.traces.size());
  CancellationCheck cancelled(options.cancelled, "token replay was cancelled", 16);
  for (const auto& trace : event_log.traces) {
    cancelled();
    TokenReplayTraceDiagnostics row;
    row.trace_index = result.size();
    row.case_id = trace.case_id;
    std::vector<std::optional<std::size_t>> ids;
    std::vector<std::optional<std::string>> names;
    for (const auto& event : trace.events) {
      const auto activity = selected_replay_activity(event, activity_attribute);
      names.push_back(activity.display_name);
      ids.push_back(activity.model_label
                        ? replayer.activity_id(*activity.model_label)
                        : full_result && !activity.display_name ? replayer.null_activity_id()
                                                              : std::nullopt);
    }
    Replayer::Counts counts;
    if (options.variant == TokenReplayVariant::kBackwards) {
      auto outcome = replayer.replay_backwards(
          ids, nullptr, options.stop_immediately_unfit,
          options.walk_through_hidden_transitions, &row);
      row.trace_is_fit = outcome.fitting;
      counts = std::move(outcome.counts);
    } else {
      counts = replayer.replay(ids, nullptr, options, &row);
      row.trace_is_fit = counts.forward_fitting(options);
    }
    if (calculate_fitness) {
      try {
        row.trace_fitness = counts.fitness();
      } catch (const std::overflow_error& error) {
        if (std::string_view(error.what()) ==
            "token integer does not fit floating-point range")
          throw std::overflow_error("int too large to convert to float");
        throw;
      }
    }
    row.produced = counts.produced;
    row.consumed = counts.consumed;
    row.missing = counts.missing;
    row.remaining = counts.remaining;
    row.places = std::move(counts.places);
    if (enabled_transitions)
      row.enabled_transitions = replayer.enabled_transition_ids(
          row.reached_marking, options);
    for (const auto index : row.unknown_event_indices) {
      row.unknown_activities.push_back(names[index].value_or(""));
      row.unknown_activity_is_null.push_back(!names[index]);
    }
    result.push_back(std::move(row));
  }
  return result;
}

}  // namespace

std::vector<TokenReplayTraceDiagnostics> replay_token_traces(
    const EventLog& event_log, const PetriNet& net, TokenReplayOptions options) {
  const bool carry = options.variant == TokenReplayVariant::kForward &&
      options.consider_activities_not_in_model_in_fitness;
  const auto activity_attribute = options.activity_attribute;
  auto rows = replay_token_trace_rows(event_log, net, std::move(options), true);
  if (!carry) return rows;
  // Variants replay by descending frequency, ties by descending activity tuple.
  using Key = std::vector<std::optional<std::string>>;
  std::map<Key, std::vector<std::size_t>> by_key;
  for (std::size_t t = 0; t < event_log.traces.size(); ++t) {
    Key key;
    for (const auto& event : event_log.traces[t].events)
      key.push_back(selected_replay_activity(event, activity_attribute).display_name);
    by_key[std::move(key)].push_back(t);
  }
  std::vector<std::pair<const Key*, std::vector<std::size_t>>> variants;
  for (auto& [key, group] : by_key) variants.emplace_back(&key, std::move(group));
  std::stable_sort(variants.begin(), variants.end(), [](const auto& a, const auto& b) {
    if (a.second.size() != b.second.size()) return a.second.size() > b.second.size();
    return *b.first < *a.first;
  });
  std::vector<std::vector<std::size_t>> groups;
  for (auto& variant : variants) groups.push_back(std::move(variant.second));
  carry_unknown_activity_unfitness(rows, groups);
  return rows;
}

TokenReplayResult replay_token_result(
    const EventLog& log, const PetriNet& net, TokenReplayOptions options) {
  if (options.variant != TokenReplayVariant::kForward)
    throw std::invalid_argument("full token replay result requires forward replay");
  validate_token_replay_event_limit(log, options);
  const auto transition_name_values = detail::replay_transition_names(
      options.nullable_transition_names, options.transition_names, net);
  detail::validate_forward_transition_name_order(transition_name_values);
  if (options.place_fitness_order) {
    std::vector<bool> listed(net.place_count(), false);
    if (options.place_fitness_order->size() != listed.size())
      throw std::invalid_argument("place_fitness_order must contain every root place exactly once");
    for (const auto place : *options.place_fitness_order) {
      if (place >= listed.size() || listed[place])
        throw std::invalid_argument("place_fitness_order must contain every root place exactly once");
      listed[place] = true;
    }
  }
  const auto& selector = options.activity_attribute;
  // Forward replay resolves selected keys across the complete log before
  // constructing hashable variants, including when grouping is disabled.
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events)
      (void)selected_replay_activity(event, selector);
  struct FullVariant {
    std::vector<EventAttribute> activities;
    std::vector<std::size_t> sources;
  };
  std::vector<FullVariant> variants;
  std::unordered_map<std::string, std::vector<std::size_t>> variant_buckets;
  const auto selected_cell = [&](const Event& event) {
    const auto activity = selected_replay_activity(event, selector);
    auto cell = native_event_attribute(event, selector);
    if (!cell) cell = EventAttribute{selector, activity.display_name.value_or(""),
                                    AttributeKind::kString, !activity.display_name};
    cell->key = selector;
    cell->first_class_role = FirstClassAttributeRole::kNone;
    cell->first_class_source_key.clear();
    if (!root_cause_key_is_hashable(*cell))
      throw std::invalid_argument("unhashable activity variant key");
    return *cell;
  };
  for (std::size_t source = 0; source < log.traces.size(); ++source) {
    FullVariant candidate;
    std::string fingerprint;
    for (const auto& event : log.traces[source].events) {
      candidate.activities.push_back(selected_cell(event));
      append_identity_field(fingerprint, 'a',
          root_cause_scalar_key_fingerprint(candidate.activities.back()));
    }
    auto& bucket = variant_buckets[fingerprint];
    auto selected = variants.size();
    for (const auto index : bucket) {
      const auto& existing = variants[index].activities;
      if (existing.size() != candidate.activities.size()) continue;
      bool equal = true;
      for (std::size_t i = 0; i < existing.size(); ++i)
        if (!root_cause_scalar_keys_equal(existing[i], candidate.activities[i])) { equal = false; break; }
      if (equal) { selected = index; break; }
    }
    if (selected == variants.size()) {
      bucket.push_back(selected);
      variants.push_back(std::move(candidate));
    }
    variants[selected].sources.push_back(source);
  }
  // Frequency ties compare activity tuples. Different frequencies do not need
  // cross-type ordering. Numeric equality includes bool/int/integral floats.
  const auto less = [&](auto&& self, const EventAttribute& left,
                         const EventAttribute& right) -> bool {
    if (root_cause_scalar_keys_equal(left, right)) return false;
    if (left.container == AttributeContainerKind::kTuple &&
        right.container == AttributeContainerKind::kTuple) {
      for (std::size_t i = 0; i < std::min(left.children.size(), right.children.size()); ++i) {
        if (root_cause_scalar_keys_equal(left.children[i], right.children[i])) continue;
        return self(self, left.children[i], right.children[i]);
      }
      return left.children.size() < right.children.size();
    }
    const auto nan = [](const EventAttribute& cell) {
      return cell.kind == AttributeKind::kFloat && std::isnan(native_attribute_number(cell));
    };
    if ((nan(left) && native_scalar_order_kind(right) == NativeScalarOrderKind::kNumeric) ||
        (nan(right) && native_scalar_order_kind(left) == NativeScalarOrderKind::kNumeric)) return false;
    if ((left.is_null && native_scalar_order_kind(right) == NativeScalarOrderKind::kString) ||
        (right.is_null && native_scalar_order_kind(left) == NativeScalarOrderKind::kString))
      throw std::invalid_argument("'<' not supported between instances of 'str' and 'NoneType'");
    try { return native_scalar_key_compare(left, right) < 0; }
    catch (const std::invalid_argument&) {
      throw std::invalid_argument("unordered activity variant keys");
    }
  };
  std::vector<std::size_t> order;
  for (std::size_t i = 0; i < variants.size(); ++i) order.push_back(i);
  const auto ordered_before = [&](auto left, auto right) {
    const auto& a = variants[left]; const auto& b = variants[right];
    if (a.sources.size() != b.sources.size()) return a.sources.size() > b.sources.size();
    for (std::size_t i = 0; i < std::min(a.activities.size(), b.activities.size()); ++i) {
      if (root_cause_scalar_keys_equal(a.activities[i], b.activities[i])) continue;
      return less(less, b.activities[i], a.activities[i]);
    }
    return a.activities.size() > b.activities.size();
  };
  // Scalar NaNs have a partial order. Explicit stable merging keeps the
  // comparisons bounded without requiring strict weak ordering from scalars.
  std::vector<std::size_t> scratch(order.size());
  for (std::size_t width = 1; width < order.size();) {
    for (std::size_t begin = 0; begin < order.size();) {
      const auto middle = begin + std::min(width, order.size() - begin);
      const auto end = middle + std::min(width, order.size() - middle);
      auto left = begin, right = middle;
      for (auto out = begin; out < end; ++out) {
        if (left == middle || (right < end && ordered_before(order[right], order[left])))
          scratch[out] = order[right++];
        else scratch[out] = order[left++];
      }
      begin = end;
    }
    order.swap(scratch);
    if (width >= order.size() - width) break;
    width += width;
  }
  const bool grouped = !options.disable_variants.value_or(options.enable_pltr_fitness);
  EventLog replay_log;
  std::vector<std::size_t> group_for_source(log.traces.size()), first_source, frequencies;
  if (grouped) for (const auto& variant : variants) {
    Trace generated;
    for (const auto& activity : variant.activities) {
      Event event;
      event.attributes.push_back(activity);
      generated.events.push_back(std::move(event));
    }
    for (const auto source : variant.sources) group_for_source[source] = replay_log.traces.size();
    replay_log.traces.push_back(std::move(generated));
    first_source.push_back(variant.sources.front());
    frequencies.push_back(variant.sources.size());
  }
  else {
    replay_log = log;
    for (std::size_t source = 0; source < log.traces.size(); ++source) {
      group_for_source[source] = source;
      first_source.push_back(source);
      frequencies.push_back(1);
    }
  }
  const auto rows = replay_token_trace_rows(replay_log, net, options, true, true);
  const auto markings = resolve_replay_markings(net, options.initial_marking, options.final_marking, &options);
  TokenReplayResult result;
  result.marking_domain = markings.domain;
  result.return_names = options.return_names;
  result.place_names = detail::replay_place_names(options.place_names, net);
  result.transition_names = detail::replay_element_names(
      options.nullable_transition_names ? std::vector<std::string>{} : options.transition_names,
      net.transitions().size(), "t", "transition_names");
  if (options.nullable_transition_names || (options.transition_names.empty() && net.transition_names())) {
    result.nullable_transition_names = transition_name_values;
    for (std::size_t t = 0; t < transition_name_values.size(); ++t)
      if (transition_name_values[t]) result.transition_names[t] = *transition_name_values[t];
  }
  for (const auto& transition : net.transitions())
    result.transition_labels.push_back(transition.activity
        ? std::optional<std::string>(net.activities().at(*transition.activity)) : std::nullopt);
  for (std::size_t source = 0; source < log.traces.size(); ++source) {
    auto row = rows[group_for_source[source]];
    row.trace_index = source;
    row.case_id = log.traces[source].case_id;
    result.traces.push_back(std::move(row));
  }
  if (options.consider_activities_not_in_model_in_fitness) {
    std::vector<std::vector<std::size_t>> groups;
    if (grouped) for (const auto variant : order) groups.push_back(variants[variant].sources);
    else for (std::size_t source = 0; source < log.traces.size(); ++source) groups.push_back({source});
    carry_unknown_activity_unfitness(result.traces, groups);
  }
  if (!options.enable_pltr_fitness) return result;

  auto& fitness = result.pltr_fitness.emplace();
  fitness.places.resize(net.place_count());
  fitness.place_order = options.place_fitness_order;
  std::vector<std::optional<std::size_t>> transition_slots(net.transitions().size());
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    if (transition.silent() || net.activities()[*transition.activity].empty()) continue;
    transition_slots[t] = fitness.transitions.size();
    TokenReplayTransitionFitness entry;
    entry.transition_index = t;
    fitness.transitions.push_back(std::move(entry));
  }
  const auto& final_marking = markings.final;
  const auto final_keys = resolve_domain_marking_keys(
      options.final_marking_keys, markings.final_keys, final_marking, net.place_count(), "final_marking_keys");
  const auto initial_keys = resolve_domain_marking_keys(
      options.initial_marking_keys, markings.initial_keys, markings.initial, net.place_count(), "initial_marking_keys");
  std::unordered_map<std::string, std::vector<std::size_t>> trace_buckets;
  const auto add_context = [](std::vector<TokenReplayTraceContexts>& entries,
                              std::size_t reference, const Event& context, bool replace) {
    auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
      return entry.trace_reference == reference;
    });
    if (found == entries.end()) {
      entries.push_back({reference, {context}});
    } else if (replace) found->contexts = {context};
    else found->contexts.push_back(context);
  };
  const auto add_member = [](std::vector<std::size_t>& members, std::size_t reference) {
    if (std::find(members.begin(), members.end(), reference) == members.end())
      members.push_back(reference);
  };
  std::vector<std::size_t> fitness_groups;
  fitness_groups.reserve(rows.size());
  for (const auto variant : order) {
    if (grouped) fitness_groups.push_back(variant);
    else fitness_groups.insert(fitness_groups.end(), variants[variant].sources.begin(),
                               variants[variant].sources.end());
  }
  for (const auto group : fitness_groups) {
    const auto& row = rows[group];
    const auto& trace = replay_log.traces[group];
    auto& bucket = trace_buckets[root_cause_trace_fingerprint(trace)];
    std::size_t reference = fitness.trace_references.size();
    for (const auto candidate : bucket) {
      if (root_cause_traces_equal(fitness.trace_references[candidate].value, trace)) {
        reference = candidate;
        break;
      }
    }
    if (reference == fitness.trace_references.size()) {
      fitness.trace_references.push_back({grouped ? std::nullopt
          : std::optional<std::size_t>(first_source[group]), trace});
      bucket.push_back(reference);
    }
    const auto context = detail::token_replay_context(trace, row);
    const TokenInteger frequency(frequencies[group]);
    for (std::size_t p = 0; p < row.places.size(); ++p) {
      if (p >= net.place_count()) {
        if (!initial_keys[p] && !final_keys[p] && !row.reached_marking_keys[p]) continue;
        const auto& key = markings.domain->places.at(p);
        const auto found = std::find_if(markings.domain->detached_places.begin(),
            markings.domain->detached_places.end(), [&](const auto& place) { return place.id == key.id; });
        throw TokenReplayPlaceKeyError(key, found->name);
      }
      const auto& counts = row.places[p];
      const auto final_missing = final_keys[p]
          ? std::max(TokenInteger(0), final_marking[p] - row.reached_marking[p])
          : TokenInteger(0);
      const auto event_missing = counts.missing - final_missing;
      auto& place = fitness.places[p];
      place.missing += event_missing + final_missing * frequency;
      place.remaining += counts.remaining * frequency;
      place.consumed += counts.consumed * frequency;
      place.produced += counts.produced * frequency;
      if (event_missing > 0) add_member(place.underfed_traces, reference);
      else if (std::find(row.cleaned_place_indices.begin(), row.cleaned_place_indices.end(), p)
                   != row.cleaned_place_indices.end() ||
               (row.reached_marking_keys[p] && (!final_keys[p] || counts.remaining != 0)))
        add_member(place.overfed_traces, reference);
    }
    const std::set<std::size_t> unknown(row.unknown_event_indices.begin(),
                                        row.unknown_event_indices.end());
    for (const auto& event : row.event_diagnostics) {
      if (unknown.contains(event.event_index)) {
        auto activity = native_event_attribute(trace.events[event.event_index], selector);
        if (!activity) {
          const auto selected = selected_replay_activity(trace.events[event.event_index], selector);
          activity = EventAttribute{selector, selected.display_name.value_or(""),
                                   AttributeKind::kString, !selected.display_name};
        }
        auto found = std::find_if(fitness.unknown_activities.begin(),
            fitness.unknown_activities.end(), [&](const auto& entry) {
              return root_cause_scalar_keys_equal(entry.activity, *activity);
            });
        if (found == fitness.unknown_activities.end()) {
          fitness.unknown_activities.push_back({std::move(*activity), {}});
          found = std::prev(fitness.unknown_activities.end());
        }
        add_context(found->traces, reference, context, true);
        continue;
      }
      if (!event.transitions_with_problems.empty()) {
        if (row.replay_stopped && event.event_index + 1 == row.replayed_events) continue;
        const auto t = event.transitions_with_problems.back();
        if (transition_slots[t])
          add_context(fitness.transitions[*transition_slots[t]].underfed_traces,
                      reference, context, false);
      } else {
        for (auto it = event.activated_transitions.rbegin();
             it != event.activated_transitions.rend(); ++it) {
          if (!transition_slots[*it]) continue;
          add_context(fitness.transitions[*transition_slots[*it]].fit_traces,
                      reference, context, false);
          break;
        }
      }
    }
  }
  return result;
}

std::vector<std::vector<std::size_t>> replay_token_activation_sequences(
    const EventLog& event_log, const PetriNet& net, TokenReplayOptions options) {
  const auto rows = replay_token_trace_rows(event_log, net, std::move(options), false);
  std::vector<std::vector<std::size_t>> result;
  result.reserve(rows.size());
  for (const auto& row : rows) result.push_back(row.activated_transitions);
  return result;
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const PetriNet& net) {
  return check_alignment(event_log, net, AlignmentOptions{});
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const PetriNet& net,
                                const AlignmentOptions& input_options) {
  AlignmentResult result;
  AlignmentOptions options = input_options;
  result.variant = options.variant;
  const std::string activity_attribute =
      options.activity_attribute.empty() ? "concept:name"
                                         : options.activity_attribute;
  result.activity_attribute = activity_attribute;
  result.trace_count = event_log.trace_count();
  if (!valid_alignment_variant(options.variant) ||
      !valid_alignment_cost_profile(options.cost_profile) ||
      !valid_explicit_marking(net, options.initial_marking) ||
      !valid_explicit_marking(net, options.final_marking)) {
    result.computed = false;
    return result;
  }
  if (result.trace_count == 0) return result;
  const auto total_start = std::chrono::steady_clock::now();
  const auto valid_table = [](const std::unordered_map<std::string, double>& table) {
    for (const auto& [_, value] : table)
      if (!std::isfinite(value) || value < 0.0) return false;
    return true;
  };
  const auto valid_vector = [](const std::vector<double>& values) {
    for (const double value : values)
      if (!std::isfinite(value) || value < 0.0) return false;
    return true;
  };
  const auto valid_optional = [](const std::optional<double>& value) {
    return !value || (std::isfinite(*value) && *value >= 0.0);
  };
  if (!valid_optional(options.default_trace_cost) ||
      !valid_optional(options.default_model_cost) ||
      !valid_optional(options.default_sync_cost) ||
      !valid_optional(options.default_silent_cost) ||
      !valid_table(options.trace_costs) || !valid_table(options.model_costs) ||
      !valid_table(options.sync_costs) ||
      !valid_vector(options.trace_costs_by_position) ||
      !valid_vector(options.model_costs_by_transition) ||
      !valid_vector(options.sync_costs_by_transition) ||
      (options.variant == AlignmentVariant::kDiscountedAStar &&
       (!std::isfinite(options.exponent) || options.exponent <= 0.0))) {
    result.computed = false;
    return result;
  }
  bool callback_invalid = false;
  if (options.model_cost_function || options.sync_cost_function) {
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& tr = net.transitions()[t];
      const std::string name = tr.silent() ? std::string{} : net.activities()[*tr.activity];
      if (options.model_cost_function) {
        const double value = options.model_cost_function(name, t);
        if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
        options.model_costs_by_transition.resize(net.transitions().size());
        options.model_costs_by_transition[t] = value;
      }
      if (options.sync_cost_function && !tr.silent()) {
        const double value = options.sync_cost_function(name, t);
        if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
        options.sync_costs_by_transition.resize(net.transitions().size());
        options.sync_costs_by_transition[t] = value;
      }
    }
    options.model_cost_function = {};
    options.sync_cost_function = {};
  }
  if (callback_invalid) {
    result.computed = false;
    return result;
  }
  const double preflight_limit =
      options.max_total_time_seconds > 0.0 && options.max_time_seconds > 0.0
          ? std::min(options.max_total_time_seconds, options.max_time_seconds)
          : (options.max_total_time_seconds > 0.0 ? options.max_total_time_seconds
                                                   : options.max_time_seconds);
  const auto preflight_deadline =
      preflight_limit > 0.0
          ? total_start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                             std::chrono::duration<double>(preflight_limit))
          : std::chrono::steady_clock::time_point::max();
  // Whether a search that stopped short stopped on a time limit: the total
  // limit, or the per-search limit of the call that began at `call_start`,
  // has run out. The searches compare their elapsed time with these same
  // limits, so a time stop always satisfies this and a state-bound stop
  // only does when the clock had run out as well.
  const auto out_of_time = [&](std::chrono::steady_clock::time_point call_start,
                               double call_limit) {
    const auto now = std::chrono::steady_clock::now();
    return (options.max_total_time_seconds > 0.0 &&
            std::chrono::duration<double>(now - total_start).count() >=
                options.max_total_time_seconds) ||
           (call_limit > 0.0 &&
            std::chrono::duration<double>(now - call_start).count() >= call_limit);
  };
  const Aligner aligner(net, options, preflight_deadline);
  if (!aligner.reachable()) {
    result.computed = false;
    result.exhausted = aligner.reachability_exhausted();
    result.time_limit_reached =
        result.exhausted && out_of_time(total_start, preflight_limit);
    return result;
  }
  const auto variants = collect_variants(event_log, aligner,
                                          activity_attribute, options.trace_order);
  std::vector<std::vector<Marking>> variant_markings;
  variant_markings.reserve(variants.size());
  double total_worst = 0.0;
  double weighted_empty = 0.0;
  AlignmentOptions baseline_options = options;
  if (options.max_total_time_seconds > 0.0) {
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - total_start).count();
    const double remaining = options.max_total_time_seconds - elapsed;
    if (remaining <= 0.0) {
      result.computed = false;
      result.exhausted = true;
      result.time_limit_reached = true;
      return result;
    }
    if (baseline_options.max_time_seconds <= 0.0 ||
        remaining < baseline_options.max_time_seconds)
      baseline_options.max_time_seconds = remaining;
  }
  const auto baseline_start = std::chrono::steady_clock::now();
  if (options.variant == AlignmentVariant::kDiscountedAStar ||
      options.model_cost_function || options.sync_cost_function || options.trace_cost_function) {
    const auto empty = aligner.align_configured({}, {}, baseline_options);
    if (empty.primary.capped) {
      result.computed = false;
      result.exhausted = true;
      result.time_limit_reached =
          out_of_time(baseline_start, baseline_options.max_time_seconds);
      return result;
    }
    weighted_empty = empty.primary.cost;
  } else {
    weighted_empty = aligner.weighted_empty_run_cost(baseline_options);
  }
  if (!std::isfinite(weighted_empty)) {
    result.computed = false;
    result.exhausted = true;
    result.time_limit_reached =
        out_of_time(baseline_start, baseline_options.max_time_seconds);
    return result;
  }
  const double baseline_model_cost = weighted_empty;
  // The variant searches are independent and deterministic, so a large log
  // runs them on up to four threads and the loop below consumes the
  // outcomes in variant order, exactly as it consumes sequential ones (a
  // failure stops the helpers from starting later variants). Cost callbacks,
  // which need not be thread-safe, keep the sequential loop; the caller's
  // cancellation callback is called under a lock.
  constexpr std::size_t kParallelAlignmentVariants = 32;
  constexpr std::size_t kMaxAlignmentHelpers = 3;
  const std::size_t hardware_threads =
      std::max<std::size_t>(1, std::thread::hardware_concurrency());
  const bool parallel_variants =
      variants.size() >= kParallelAlignmentVariants && hardware_threads > 1 &&
      !options.model_cost_function && !options.sync_cost_function &&
      !options.trace_cost_function;
  struct PrecomputedVariant {
    bool stopped = false;  // out of total time or cancelled before it began
    std::exception_ptr error;
    Aligner::ConfiguredOutcome outcome;
    std::chrono::steady_clock::time_point start;
    double time_limit = 0.0;
  };
  std::vector<PrecomputedVariant> precomputed;
  std::mutex cancel_mutex;
  std::function<bool()> locked_cancel;
  if (parallel_variants) {
    if (options.cancelled)
      locked_cancel = [&] {
        const std::lock_guard<std::mutex> lock(cancel_mutex);
        return options.cancelled();
      };
    precomputed.resize(variants.size());
    std::atomic<std::size_t> next_variant{0};
    std::atomic<std::size_t> first_stop{variants.size()};
    const auto stop_at = [&](std::size_t index) {
      std::size_t current = first_stop.load();
      while (index < current && !first_stop.compare_exchange_weak(current, index)) {}
    };
    const auto work = [&] {
      for (;;) {
        const std::size_t index = next_variant.fetch_add(1);
        if (index >= variants.size() || index > first_stop.load()) return;
        auto& slot = precomputed[index];
        try {
          if ((options.max_total_time_seconds > 0.0 &&
               std::chrono::duration<double>(std::chrono::steady_clock::now() - total_start)
                       .count() >= options.max_total_time_seconds) ||
              (locked_cancel && locked_cancel())) {
            slot.stopped = true;
            stop_at(index);
            continue;
          }
          AlignmentOptions trace_options = options;
          trace_options.cancelled = locked_cancel;
          if (options.max_total_time_seconds > 0.0) {
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - total_start).count();
            const double remaining = std::max(0.0, options.max_total_time_seconds - elapsed);
            if (trace_options.max_time_seconds <= 0.0 || remaining < trace_options.max_time_seconds)
              trace_options.max_time_seconds = remaining;
          }
          slot.time_limit = trace_options.max_time_seconds;
          slot.start = std::chrono::steady_clock::now();
          slot.outcome = aligner.align_configured(variants[index].ids, variants[index].names,
                                                  trace_options);
          if (slot.outcome.invalid_cost || slot.outcome.primary.capped) stop_at(index);
        } catch (...) {
          slot.error = std::current_exception();
          stop_at(index);
        }
      }
    };
    std::vector<std::thread> helpers;
    struct JoinHelpers {
      std::vector<std::thread>& threads;
      ~JoinHelpers() {
        for (auto& thread : threads)
          if (thread.joinable()) thread.join();
      }
    } join_helpers{helpers};
    const std::size_t helper_count = std::min<std::size_t>(
        {kMaxAlignmentHelpers, hardware_threads - 1, variants.size() - 1});
    for (std::size_t k = 0; k < helper_count; ++k) {
      try {
        helpers.emplace_back(work);
      } catch (...) {
        break;  // fewer threads; the calling thread takes the rest
      }
    }
    work();
  }
  for (std::size_t variant_index = 0; variant_index < variants.size(); ++variant_index) {
    const auto& variant = variants[variant_index];
    AlignmentOptions trace_options = options;
    std::chrono::steady_clock::time_point trace_start;
    Aligner::ConfiguredOutcome configured;
    if (parallel_variants) {
      auto& slot = precomputed[variant_index];
      if (slot.error) std::rethrow_exception(slot.error);
      if (slot.stopped) {
        result.computed = false;
        result.exhausted = true;
        result.time_limit_reached = out_of_time(total_start, 0.0);
        return result;
      }
      trace_options.max_time_seconds = slot.time_limit;
      trace_start = slot.start;
      configured = std::move(slot.outcome);
    } else {
      if ((options.max_total_time_seconds > 0.0 &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - total_start)
                   .count() >= options.max_total_time_seconds) ||
          (options.cancelled && options.cancelled())) {
        result.computed = false;
        result.exhausted = true;
        result.time_limit_reached = out_of_time(total_start, 0.0);
        return result;
      }
      if (options.max_total_time_seconds > 0.0) {
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - total_start).count();
        const double remaining = std::max(0.0, options.max_total_time_seconds - elapsed);
        if (trace_options.max_time_seconds <= 0.0 || remaining < trace_options.max_time_seconds)
          trace_options.max_time_seconds = remaining;
      }
      if (trace_options.trace_cost_function) {
        trace_options.trace_costs_by_position.resize(variant.names.size());
        for (std::size_t i = 0; i < variant.names.size(); ++i) {
          trace_options.trace_costs_by_position[i] =
              trace_options.trace_cost_function(variant.names[i], i);
          if (!std::isfinite(trace_options.trace_costs_by_position[i]) ||
              trace_options.trace_costs_by_position[i] < 0.0) {
            result.computed = false;
            result.exhausted = false;
            return result;
          }
        }
        trace_options.trace_cost_function = {};
      }
      trace_start = std::chrono::steady_clock::now();
      configured = aligner.align_configured(variant.ids, variant.names, trace_options);
    }
    result.diagnostics.visited_states += configured.diagnostics.visited_states;
    result.diagnostics.queued_states = std::max(
        result.diagnostics.queued_states, configured.diagnostics.queued_states);
    result.diagnostics.traversed_arcs += configured.diagnostics.traversed_arcs;
    result.diagnostics.lp_solved += configured.diagnostics.lp_solved;
    result.diagnostics.closed_set_length = std::max(
        result.diagnostics.closed_set_length, configured.diagnostics.closed_set_length);
    result.diagnostics.num_visited_markings = std::max(
        result.diagnostics.num_visited_markings,
        configured.diagnostics.num_visited_markings);
    if (configured.invalid_cost) {
      result.computed = false;
      result.exhausted = false;
      return result;
    }
    if (configured.primary.capped) {
      result.computed = false;
      result.exhausted = true;
      result.time_limit_reached =
          out_of_time(trace_start, trace_options.max_time_seconds);
      result.variants.clear();
      return result;
    }
    const double profile_trace = options.default_trace_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    double trace_worst = 0.0;
    for (std::size_t i = 0; i < variant.names.size(); ++i) {
      const auto trace_it = options.trace_costs.find(variant.names[i]);
      double value = trace_it == options.trace_costs.end() ? profile_trace : trace_it->second;
      if (i < trace_options.trace_costs_by_position.size())
        value = trace_options.trace_costs_by_position[i];
      trace_worst += value;
    }
    const double worst = trace_worst + baseline_model_cost;
    const double fitness = worst <= 1e-12
                               ? 1.0
                               : std::clamp(1.0 - configured.primary.cost / worst, 0.0, 1.0);
    result.total_cost += configured.primary.cost * variant.count;
    total_worst += worst * variant.count;
    if (configured.primary.cost <= 1e-9) result.fitting_traces += variant.count;
    VariantAlignment item{variant.names, variant.count,
                          std::move(configured.primary.moves),
                          configured.primary.cost, fitness};
    item.diagnostics = configured.diagnostics;
    item.worst_cost = worst;
    item.projected_model_moves = configured.primary.projected_model_moves;
    item.complete_moves = configured.primary.complete_moves.empty()
                              ? item.moves
                              : std::move(configured.primary.complete_moves);
    item.optimal_alignments.push_back(item.moves);
    for (auto& alt : configured.alternatives)
      item.optimal_alignments.push_back(std::move(alt));
    result.variants.push_back(std::move(item));
    variant_markings.push_back(std::move(configured.primary.prefix_markings));
  }
  result.fitness = total_worst <= 1e-12
                     ? 1.0
                     : std::clamp(1.0 - result.total_cost / total_worst, 0.0, 1.0);
  const Replayer replayer(net, options.initial_marking, options.final_marking);
  try {
    result.precision = compute_alignment_precision(variants, variant_markings, replayer);
  } catch (const detail::SilentSearchBudgetExhausted&) {
    result.precision = std::numeric_limits<double>::quiet_NaN();
    result.precision_computed = false;
  }
  std::stable_sort(result.variants.begin(), result.variants.end(),
                   [](const VariantAlignment& a, const VariantAlignment& b) {
                     if (a.fitness != b.fitness) return a.fitness < b.fitness;
                     return a.count > b.count;
                   });
  return result;
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const ProcessTree& tree) {
  // A process tree always translates to a sound workflow net, so this path
  // never reports computed == false.
  return check_alignment(event_log, to_petri_net(tree));
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const ProcessTree& tree,
                                const AlignmentOptions& options) {
  return check_alignment(event_log, to_petri_net(tree), options);
}

std::vector<ActivityDeviation> summarize_alignment_moves(
    const AlignmentResult& alignment) {
  std::unordered_map<std::string, ActivityDeviation> by_activity;
  for (const auto& variant : alignment.variants) {
    for (const auto& move : variant.moves) {
      if (move.activity.empty()) continue;  // silent tau routing is not an activity deviation
      auto& tally = by_activity[move.activity];
      if (tally.activity.empty()) tally.activity = move.activity;
      switch (move.type) {
        case AlignmentMoveType::kSynchronous:
          tally.synchronous += variant.count;
          break;
        case AlignmentMoveType::kLog:
          tally.log_moves += variant.count;
          break;
        case AlignmentMoveType::kModel:
          tally.model_moves += variant.count;
          break;
      }
    }
  }

  std::vector<ActivityDeviation> result;
  result.reserve(by_activity.size());
  for (auto& [name, tally] : by_activity) {
    result.push_back(std::move(tally));
  }
  std::sort(result.begin(), result.end(),
            [](const ActivityDeviation& a, const ActivityDeviation& b) {
              const auto deviations_a = a.log_moves + a.model_moves;
              const auto deviations_b = b.log_moves + b.model_moves;
              if (deviations_a != deviations_b) {
                return deviations_a > deviations_b;
              }
              if (a.synchronous != b.synchronous) {
                return a.synchronous > b.synchronous;
              }
              return a.activity < b.activity;
            });
  return result;
}

std::vector<std::optional<std::vector<std::vector<std::size_t>>>>
token_replay_firing_sequences(const PetriNet& net,
                              const std::vector<std::vector<std::string>>& variants,
                              const std::function<bool()>& cancelled) {
  // One trace per variant, replayed exactly as replay_token_traces replays a
  // log of them with default options (forward, final routing through hidden
  // transitions, activities outside the model skipped).
  EventLog variant_log;
  variant_log.traces.reserve(variants.size());
  for (std::size_t v = 0; v < variants.size(); ++v) {
    Trace trace;
    trace.case_id = std::to_string(v);
    trace.events.reserve(variants[v].size());
    for (const auto& activity : variants[v]) {
      Event event;
      event.case_id = trace.case_id;
      event.case_id_state = FirstClassAttributeState::kPresent;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    variant_log.traces.push_back(std::move(trace));
  }
  TokenReplayOptions options;
  options.cancelled = cancelled;
  const auto rows = replay_token_trace_rows(variant_log, net, options, true, false,
                                            /*enabled_transitions=*/false);
  std::vector<std::optional<std::vector<std::vector<std::size_t>>>> result(variants.size());
  for (const auto& row : rows) {
    if (row.trace_index >= variants.size()) continue;
    // An event outside the model is skipped (its list is empty), as the
    // replay's fitness does; it does not make the trace deviate.
    if (!row.trace_is_fit || row.missing != 0 || row.remaining != 0 ||
        row.replay_stopped ||
        row.event_diagnostics.size() != variants[row.trace_index].size())
      continue;
    std::vector<std::vector<std::size_t>> steps;
    steps.reserve(row.event_diagnostics.size() + 1);
    std::size_t used = 0;
    bool consistent = true;
    for (const auto& event : row.event_diagnostics) {
      // The trace-level list is the events' lists in order, then the final
      // routing; anything else would not be one firing sequence.
      if (used + event.activated_transitions.size() > row.activated_transitions.size() ||
          !std::equal(event.activated_transitions.begin(), event.activated_transitions.end(),
                      row.activated_transitions.begin() + static_cast<std::ptrdiff_t>(used))) {
        consistent = false;
        break;
      }
      used += event.activated_transitions.size();
      steps.push_back(event.activated_transitions);
    }
    if (!consistent) continue;
    steps.emplace_back(row.activated_transitions.begin() + static_cast<std::ptrdiff_t>(used),
                       row.activated_transitions.end());
    result[row.trace_index] = std::move(steps);
  }
  return result;
}

std::vector<std::optional<std::vector<std::vector<std::size_t>>>>
replay_firing_sequences(const PetriNet& net,
                        const std::vector<std::vector<std::string>>& variants,
                        const std::function<bool()>& cancelled,
                        std::vector<bool>* capped) {
  const Replayer replayer(net);
  CancellationCheck check_cancelled(cancelled, "model replay was cancelled", 8);
  // Each cached step holds two markings; a log whose variants keep reaching
  // new markings would otherwise grow the cache without bound. About 256 MiB
  // of markings, estimated from the place count, is kept between variants.
  const std::size_t marking_bytes =
      2 * (net.place_count() * sizeof(TokenInteger) + sizeof(Replayer::Marking)) + 128;
  const std::size_t cache_limit =
      std::max<std::size_t>(4096, (std::size_t{256} << 20) / marking_bytes);
  std::vector<std::optional<std::vector<std::vector<std::size_t>>>> result;
  result.reserve(variants.size());
  if (capped) capped->assign(variants.size(), false);
  for (std::size_t v = 0; v < variants.size(); ++v) {
    const auto& variant = variants[v];
    check_cancelled();
    std::vector<std::optional<std::size_t>> ids;
    ids.reserve(variant.size());
    for (const auto& name : variant) {
      ids.push_back(replayer.activity_id(name));
    }
    bool variant_capped = false;
    result.push_back(replayer.clean_firing_sequence(ids, &variant_capped));
    if (capped && !result.back()) (*capped)[v] = variant_capped;
    if (replayer.cached_steps() > cache_limit) replayer.clear_step_caches();
  }
  return result;
}

namespace {

// The distinct markings a prefix walk reaches, by id in first-reached order.
// A marking of small counts is kept as its counts' zigzag varints in one
// byte string (a byte per place for 0/1 nets); one holding a wide count is
// kept whole. Room for `capacity` markings is reserved up front and never
// moves, so other threads may decode any id published to them while the
// walk interns more.
class PrefixMarkingPool {
 public:
  explicit PrefixMarkingPool(std::size_t capacity) : capacity_(capacity) {
    slots_.reserve(capacity);
    compact_.reserve(capacity);
    wide_.reserve(capacity);
  }
  std::uint32_t intern(const TokenMarking& marking) {
    key_.clear();
    bool compact = true;
    for (const auto& count : marking) {
      if (!count.is_small()) { compact = false; break; }
      const std::int64_t value = count.small_value();
      std::uint64_t zigzag = (static_cast<std::uint64_t>(value) << 1) ^
                             static_cast<std::uint64_t>(value < 0 ? -1 : 0);
      while (zigzag >= 0x80) {
        key_.push_back(static_cast<char>((zigzag & 0x7f) | 0x80));
        zigzag >>= 7;
      }
      key_.push_back(static_cast<char>(zigzag));
    }
    if (compact) {
      if (const auto found = compact_ids_.find(key_); found != compact_ids_.end())
        return found->second;
    } else if (const auto found = wide_ids_.find(marking); found != wide_ids_.end()) {
      return found->second;
    }
    if (slots_.size() >= capacity_ || slots_.size() >= std::numeric_limits<std::uint32_t>::max())
      throw std::logic_error("token prefix walk reached more markings than prefixes");
    const auto id = static_cast<std::uint32_t>(slots_.size());
    if (compact) {
      compact_.push_back(key_);
      compact_ids_.emplace(compact_.back(), id);
      slots_.push_back({false, compact_.size() - 1, marking.size()});
    } else {
      const auto added = wide_ids_.emplace(marking, id).first;
      wide_.push_back(&added->first);
      slots_.push_back({true, wide_.size() - 1, marking.size()});
    }
    return id;
  }
  [[nodiscard]] std::size_t size() const noexcept { return slots_.size(); }
  void decode(std::size_t id, TokenMarking& marking) const {
    const auto& slot = slots_[id];
    if (slot.wide) {
      marking = *wide_[slot.index];
      return;
    }
    const auto& key = compact_[slot.index];
    marking.resize(slot.places);
    std::size_t at = 0;
    for (std::size_t place = 0; place < slot.places; ++place) {
      std::uint64_t zigzag = 0;
      for (unsigned shift = 0;; shift += 7) {
        const auto byte = static_cast<unsigned char>(key[at++]);
        zigzag |= std::uint64_t(byte & 0x7f) << shift;
        if (byte < 0x80) break;
      }
      const auto magnitude = static_cast<std::int64_t>(zigzag >> 1);
      marking[place] = TokenInteger((zigzag & 1) != 0 ? -magnitude - 1 : magnitude);
    }
  }

 private:
  struct Slot {
    bool wide;
    std::size_t index;
    std::size_t places;
  };
  std::size_t capacity_;
  std::string key_;
  std::vector<std::string> compact_;  // reserved: elements never move
  std::unordered_map<std::string_view, std::uint32_t> compact_ids_;
  std::unordered_map<TokenMarking, std::uint32_t, TokenMarkingHash> wide_ids_;
  std::vector<const TokenMarking*> wide_;
  std::vector<Slot> slots_;
};

}  // namespace

void detail::forward_token_prefix_walk_impl(
    const PetriNet& net, const TokenReplayOptions& options, const TokenPrefixTrie& trie,
    void (*visit)(void*, std::size_t, const std::vector<std::size_t>*), void* context) {
  if (options.variant != TokenReplayVariant::kForward || !options.stop_immediately_unfit ||
      options.try_to_reach_final_marking_through_hidden || options.consider_remaining_in_fitness ||
      options.consider_activities_not_in_model_in_fitness)
    throw std::logic_error("token prefix walk requires the precision replay options");
  const auto nodes = trie.parent.size();
  if (trie.label.size() != nodes)
    throw std::logic_error("token prefix trie is malformed");
  std::vector<std::size_t> depth(nodes, 0);
  for (std::size_t node = 1; node < nodes; ++node) {
    if (trie.parent[node] >= node || trie.label[node] >= trie.labels.size())
      throw std::logic_error("token prefix trie is malformed");
    depth[node] = depth[trie.parent[node]] + 1;
  }
  // The materialised prefix log holds one event per prefix step; its row
  // bound is checked over that whole log first.
  if (options.max_rows_to_materialize) {
    std::size_t available = *options.max_rows_to_materialize;
    for (const auto node : trie.scored) {
      if (depth[node] > available)
        throw std::length_error("token replay input exceeds max_rows_to_materialize");
      available -= depth[node];
    }
  }
  // The full-result checks that precede replay.
  const auto transition_name_values = detail::replay_transition_names(
      options.nullable_transition_names, options.transition_names, net);
  detail::validate_forward_transition_name_order(transition_name_values);
  if (options.place_fitness_order) {
    std::vector<bool> listed(net.place_count(), false);
    if (options.place_fitness_order->size() != listed.size())
      throw std::invalid_argument("place_fitness_order must contain every root place exactly once");
    for (const auto place : *options.place_fitness_order) {
      if (place >= listed.size() || listed[place])
        throw std::invalid_argument("place_fitness_order must contain every root place exactly once");
      listed[place] = true;
    }
  }
  // Each distinct label resolves to a model activity the way the grouped
  // variant (or, ungrouped, the event itself) resolves in the full replay.
  const auto& selector = options.activity_attribute;
  const bool grouped = !options.disable_variants.value_or(options.enable_pltr_fitness);
  std::vector<std::optional<std::string>> model_labels(trie.labels.size());
  std::vector<bool> null_labels(trie.labels.size(), false);
  for (const auto& event : trie.labels) (void)selected_replay_activity(event, selector);
  for (std::size_t index = 0; index < trie.labels.size(); ++index) {
    const auto& event = trie.labels[index];
    if (grouped) {
      const auto activity = selected_replay_activity(event, selector);
      auto cell = native_event_attribute(event, selector);
      if (!cell) cell = EventAttribute{selector, activity.display_name.value_or(""),
                                      AttributeKind::kString, !activity.display_name};
      cell->key = selector;
      cell->first_class_role = FirstClassAttributeRole::kNone;
      cell->first_class_source_key.clear();
      if (!root_cause_key_is_hashable(*cell))
        throw std::invalid_argument("unhashable activity variant key");
      Event generated;
      generated.attributes.push_back(*cell);
      const auto resolved = selected_replay_activity(generated, selector);
      model_labels[index] = resolved.model_label;
      null_labels[index] = !resolved.display_name;
    } else {
      const auto resolved = selected_replay_activity(event, selector);
      model_labels[index] = resolved.model_label;
      null_labels[index] = !resolved.display_name;
    }
  }
  const Replayer replayer(net, options.initial_marking, options.final_marking, &options, true);
  std::vector<std::optional<std::size_t>> ids(trie.labels.size());
  for (std::size_t index = 0; index < ids.size(); ++index)
    ids[index] = model_labels[index] ? replayer.activity_id(*model_labels[index])
               : null_labels[index] ? replayer.null_activity_id() : std::nullopt;

  // Replay is deterministic in its state, so a prefix replayed from the state
  // its ancestor reached matches the prefix replayed alone; a prefix whose
  // replay stopped leaves every extension at the same stopped state.
  //
  // The enabled list depends on the reached marking alone, so it is searched
  // once per distinct marking. The outcome is that of one pass which searched
  // each marking when first reached and visited each prefix in turn:
  //  - the walk interns each prefix's reached marking and stops at the first
  //    exception it raises (after the prefixes before it), or once a search
  //    has failed: nothing past the prefix that first reached the failed
  //    marking can change which exception the pass raises;
  //  - the searches are independent of each other. The calling thread
  //    searches each new marking as the walk reaches it until helper threads
  //    start: a long walk hands its later markings to up to three more
  //    threads, each with its own Replayer (whose caches are per instance),
  //    and the calling thread walks, then searches too, and alone polls for
  //    cancellation;
  //  - a helper that cannot run (its Replayer or a search runs out of memory,
  //    or any other failure of its own) drops out and leaves the marking it
  //    held to the calling thread, so helpers never change the outcome: only
  //    the calling thread's searches raise, as in the single pass;
  //  - the visits then run in prefix order, rethrowing a search's exception
  //    at the prefix that first reached its marking, and the walk's own
  //    exception last — the order the single pass raises them.
  // Equal enabled lists are shared, so the visitor sees one list per value.
  const std::size_t capacity = trie.scored.size();
  PrefixMarkingPool pool(capacity);
  std::vector<std::uint32_t> reached;  // by scored prefix: its marking id
  std::vector<char> unfit;             // by scored prefix: replay stopped
  reached.reserve(capacity);
  unfit.reserve(capacity);
  std::exception_ptr walk_error;

  constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();
  std::vector<std::uint32_t> list_of(capacity, 0);
  std::vector<std::exception_ptr> failure(capacity);
  std::deque<std::vector<std::size_t>> lists;
  std::map<std::vector<std::size_t>, std::uint32_t> list_ids;
  std::mutex lists_mutex;
  std::atomic<std::size_t> published{0};  // markings interned and readable
  std::atomic<bool> walk_done{false};
  std::atomic<std::size_t> next_search{0};
  std::atomic<std::size_t> first_failed{kNone};
  std::atomic<bool> stop_searches{false};
  std::mutex wait_mutex;
  std::condition_variable wake;
  // Searches marking `id`, keeping its list or its failure. A helper's
  // std::bad_alloc propagates instead (the helper drops out).
  const auto search_one = [&](const Replayer& searcher,
                              std::optional<Replayer::ForwardEnabledPlan>& plan,
                              TokenMarking& marking, std::size_t id, bool helper) {
    const auto keep_failure = [&] {
      failure[id] = std::current_exception();
      auto lowest = first_failed.load();
      while (id < lowest && !first_failed.compare_exchange_weak(lowest, id)) {}
    };
    try {
      pool.decode(id, marking);
      auto list = searcher.enabled_transition_ids(marking, options, std::nullopt, &plan);
      const std::lock_guard<std::mutex> lock(lists_mutex);
      const auto [it, added] =
          list_ids.try_emplace(std::move(list), static_cast<std::uint32_t>(lists.size()));
      if (added) {
        try {
          lists.push_back(it->first);
        } catch (...) {
          list_ids.erase(it);
          throw;
        }
      }
      list_of[id] = it->second;
    } catch (const std::bad_alloc&) {
      if (helper) throw;
      keep_failure();
    } catch (...) {
      keep_failure();
    }
  };
  // Takes markings in id order until none is left. `holding`, for a helper,
  // names the marking it has taken and not finished (kNone otherwise).
  const auto search_all = [&](const Replayer& searcher,
                              std::optional<Replayer::ForwardEnabledPlan>& plan,
                              CancellationCheck* check, std::size_t* holding) {
    TokenMarking marking;
    for (;;) {
      if (stop_searches.load()) return;
      const std::size_t id = next_search.fetch_add(1);
      if (holding) *holding = id;
      // Wait for the walk to reach the id's marking, or to end short of it.
      while (id >= published.load(std::memory_order_acquire)) {
        if (walk_done.load(std::memory_order_acquire)) {
          if (id >= published.load(std::memory_order_acquire)) {
            if (holding) *holding = kNone;
            return;
          }
          break;
        }
        std::unique_lock<std::mutex> lock(wait_mutex);
        wake.wait_for(lock, std::chrono::milliseconds(1), [&] {
          return stop_searches.load() || walk_done.load() ||
                 id < published.load(std::memory_order_acquire);
        });
        if (stop_searches.load()) return;
      }
      if (check) (*check)();
      // A search past the lowest failed one is never read.
      if (id <= first_failed.load()) search_one(searcher, plan, marking, id, holding != nullptr);
      if (holding) *holding = kNone;
    }
  };
  {
    // A thread joins for every kSearchesPerThread markings, up to four
    // threads in all. Each builds its own Replayer and keeps the caller's
    // silent-search budget.
    constexpr std::size_t kSearchesPerThread = 2048;
    constexpr std::size_t kMaxWorkers = 3;
    const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    const std::size_t max_workers = std::min<std::size_t>(kMaxWorkers, hardware - 1);
    const std::size_t budget = detail::enabled_search_budget();
    // dropped[k]: the marking helper k held when it dropped out, or kNone.
    std::atomic<std::size_t> dropped[kMaxWorkers];
    for (auto& slot : dropped) slot.store(kNone);
    std::vector<std::thread> workers;
    struct JoinAll {
      std::vector<std::thread>& threads;
      std::atomic<bool>& stop;
      std::atomic<bool>& done;
      std::condition_variable& wake;
      ~JoinAll() {
        stop.store(true);
        done.store(true);
        wake.notify_all();
        for (auto& thread : threads)
          if (thread.joinable()) thread.join();
      }
    } join_all{workers, stop_searches, walk_done, wake};
    bool spawn_failed = false;
    const auto publish = [&] {
      published.store(pool.size(), std::memory_order_release);
      wake.notify_all();
      while (!spawn_failed && workers.size() < max_workers &&
             pool.size() >= (workers.size() + 1) * kSearchesPerThread) {
        try {
          workers.emplace_back([&, slot = &dropped[workers.size()]] {
            std::size_t holding = kNone;
            try {
              const detail::ScopedEnabledSearchBudget scoped(budget);
              const Replayer searcher(net, options.initial_marking, options.final_marking,
                                      &options, true);
              std::optional<Replayer::ForwardEnabledPlan> plan;
              search_all(searcher, plan, nullptr, &holding);
            } catch (...) {
              // The helper's own failure, not a search's: it drops out.
            }
            slot->store(holding);
          });
        } catch (...) {
          spawn_failed = true;  // fewer threads; the calling thread takes the rest
        }
      }
    };

    std::optional<Replayer::ForwardEnabledPlan> enabled_plan;
    TokenMarking searched;
    auto state = replayer.start_prefix_walk();
    std::size_t current = 0;
    bool stopped = false;
    std::vector<std::size_t> path;
    CancellationCheck cancelled(options.cancelled, "token replay was cancelled", 64);
    for (std::size_t index = 0; index < capacity; ++index) {
      // A failed search ends the pass at the prefix that first reached its
      // marking, which the walk has passed.
      if (first_failed.load() != kNone) break;
      cancelled();
      try {
        const auto target = trie.scored[index];
        path.clear();
        auto node = target;
        while (node != 0 && depth[node] > depth[current]) {
          path.push_back(node);
          node = trie.parent[node];
        }
        if (node != current) {
          path.clear();
          for (node = target; node != 0; node = trie.parent[node]) path.push_back(node);
          state = replayer.start_prefix_walk();
          current = 0;
          stopped = false;
        }
        for (auto step = path.rbegin(); step != path.rend(); ++step) {
          if (!stopped && !replayer.advance_prefix_walk(state, ids[trie.label[*step]], options))
            stopped = true;
          current = *step;
        }
        // The full replay computes every row's fitness, which can overflow for
        // token counts beyond floating-point range.
        try {
          (void)replayer.prefix_walk_fitness(state);
        } catch (const std::overflow_error& error) {
          if (std::string_view(error.what()) == "token integer does not fit floating-point range")
            throw std::overflow_error("int too large to convert to float");
          throw;
        }
        // The full replay lists enabled transitions for every row, fit or not.
        const auto before = pool.size();
        reached.push_back(pool.intern(state.marking));
        unfit.push_back(stopped ? 1 : 0);
        if (pool.size() != before) {
          // Until helpers run, the calling thread searches a marking when
          // the walk first reaches it.
          if (workers.empty()) {
            const std::size_t id = next_search.fetch_add(1);
            search_one(replayer, enabled_plan, searched, id, false);
          }
          if (pool.size() % 64 == 0) publish();
        }
      } catch (const OperationCancelled&) {
        throw;
      } catch (...) {
        walk_error = std::current_exception();
        break;
      }
    }
    publish();
    walk_done.store(true, std::memory_order_release);
    wake.notify_all();
    search_all(replayer, enabled_plan, &cancelled, nullptr);
    for (auto& thread : workers) thread.join();
    // The markings helpers dropped, searched here in id order.
    std::vector<std::size_t> left;
    for (const auto& slot : dropped)
      if (const auto id = slot.load(); id != kNone) left.push_back(id);
    std::sort(left.begin(), left.end());
    for (const auto id : left) {
      cancelled();
      if (id <= first_failed.load()) search_one(replayer, enabled_plan, searched, id, false);
    }
  }

  std::size_t first_new = 0;
  for (std::size_t index = 0; index < reached.size(); ++index) {
    const auto id = reached[index];
    if (id == first_new) {
      ++first_new;
      if (failure[id]) std::rethrow_exception(failure[id]);
    }
    visit(context, index, unfit[index] ? nullptr : &lists[list_of[id]]);
  }
  if (walk_error) std::rethrow_exception(walk_error);
  // The checks the full result performs after replay.
  (void)replay_token_result(EventLog{}, net, options);
}

namespace {

// Structural reduction of a net for final-marking reachability, applied
// before the reachability-only searches (final_marking_reachability and
// final_marking_reachable). Every rule keeps "the final marking is reachable
// from the initial marking" exactly (both directions), so a reduced net whose
// initial marking already equals its final marking proves the original net
// reachable without searching, and a smaller residue is searched first.
//
// Only ordinary nets are loaded: unit arcs (repeated unweighted endpoints are
// multiplicities), no reset or inhibitor arcs. Labels do not matter. Rules,
// with pre/post the input/output multisets and M0/Mf the markings:
//  - a transition with pre = post (its firing changes nothing) is removed;
//  - of two transitions with equal pre and post, one is removed;
//  - a transition needing more tokens from a place than the place has
//    initially, where no transition produces into it, is dead and removed;
//  - of two places with equal producers, consumers (with multiplicities), M0
//    and Mf, one is removed (both always hold the same count);
//  - a place every transition touching it only tests (consumes and produces
//    back the same count, at most M0), with M0 = Mf, never changes and never
//    disables anything, and is removed;
//  - post-agglomeration: t with pre(t) = {p} (one token), t the only consumer
//    of p, t not producing into p, Mf(p) = 0: every producer of p produces
//    post(t) instead (k times for k tokens into p), M0(p) tokens are moved
//    through t, and t and p are removed. Firing t right after each production
//    into p (and first for the M0(p) tokens) disables nothing — no one else
//    consumes p and only adds tokens elsewhere — and every token of p must be
//    consumed by t to reach Mf(p) = 0, so runs to the final marking map both
//    ways;
//  - pre-agglomeration: t with post(t) = {q} (one token), t the only producer
//    of q, t not consuming q, M0(q) = Mf(q) = 0: every consumer of q takes
//    pre(t) instead (k times for k tokens from q), and t and q are removed.
//    Delaying each firing of t to just before the consumer of its token keeps
//    more tokens around in between (no inhibitor arcs), so runs map both ways.
// A block-structured net (the process-tree and POWL translations) reduces to
// a marking with nothing left to fire: its sequences, choices, loops and
// parallel branches fold into single places and duplicate transitions.
class ReachabilityReduction {
 public:
  enum class Outcome { kUnchanged, kTrivial, kReduced };

  // Whether the net is ordinary (see above); the reduction applies only then.
  bool load(const PetriNet& net, const Marking& initial, const Marking& final) {
    const std::size_t places = net.place_count();
    const std::size_t transitions = net.transitions().size();
    if (places >= kLimit || transitions >= kLimit || initial.size() != places ||
        final.size() != places)
      return false;
    pre_.assign(transitions, {});
    post_.assign(transitions, {});
    visible_.assign(transitions, 0);
    transition_alive_.assign(transitions, 1);
    consumers_.assign(places, {});
    producers_.assign(places, {});
    place_alive_.assign(places, 1);
    initial_.assign(initial.begin(), initial.end());
    final_.assign(final.begin(), final.end());
    const auto unit = [](const std::vector<PetriArcWeight>& weights, std::size_t arcs) {
      if (weights.empty()) return true;
      if (weights.size() != arcs) return false;
      return std::all_of(weights.begin(), weights.end(), [](const PetriArcWeight& weight) {
        return !weight.negative && weight.magnitude == TokenInteger(1);
      });
    };
    for (std::size_t t = 0; t < transitions; ++t) {
      const auto& transition = net.transitions()[t];
      if (!transition.reset_inputs.empty() || !transition.inhibitor_inputs.empty() ||
          !unit(transition.input_weights, transition.inputs.size()) ||
          !unit(transition.output_weights, transition.outputs.size()))
        return false;
      for (const auto place : transition.inputs) {
        if (place >= places) return false;
        // Weighted input arcs on one place are each checked on their own
        // (fire_petri_transition), which is not a multiplicity.
        if (!transition.input_weights.empty() && count(pre_[t], place) != 0) return false;
        add(pre_[t], static_cast<std::uint32_t>(place), 1);
      }
      for (const auto place : transition.outputs) {
        if (place >= places) return false;
        add(post_[t], static_cast<std::uint32_t>(place), 1);
      }
      for (const auto& [place, k] : pre_[t]) add(consumers_[place], static_cast<std::uint32_t>(t), k);
      for (const auto& [place, k] : post_[t]) add(producers_[place], static_cast<std::uint32_t>(t), k);
      arcs_ += pre_[t].size() + post_[t].size();
      visible_[t] = transition.silent() ? 0 : 1;
    }
    // Agglomeration may multiply arcs; past this budget it is skipped.
    arc_budget_ = 2 * arcs_ + 4096;
    return true;
  }

  Outcome reduce(const std::function<bool()>& cancelled) {
    CancelPoll poll(cancelled);
    bool changed_any = false;
    for (bool changed = true; changed;) {
      changed = false;
      if (poll()) return changed_any ? Outcome::kReduced : Outcome::kUnchanged;
      changed |= drop_transitions();
      changed |= drop_places();
      for (std::uint32_t t = 0; t < transition_alive_.size(); ++t) {
        if (!transition_alive_[t]) continue;
        if (poll()) return changed_any || changed ? Outcome::kReduced : Outcome::kUnchanged;
        changed |= post_agglomerate(t) || pre_agglomerate(t);
      }
      changed_any |= changed;
    }
    for (std::size_t p = 0; p < place_alive_.size(); ++p)
      if (place_alive_[p] && initial_[p] != final_[p])
        return changed_any ? Outcome::kReduced : Outcome::kUnchanged;
    return Outcome::kTrivial;
  }

  // Places plus transitions left.
  [[nodiscard]] std::size_t size() const {
    return static_cast<std::size_t>(std::count(place_alive_.begin(), place_alive_.end(), 1) +
                                    std::count(transition_alive_.begin(), transition_alive_.end(), 1));
  }

  // The reduced net with its markings, when they fit the search's marking type.
  bool build(PetriNet& net, Marking& initial, Marking& final) const {
    std::vector<std::size_t> renumber(place_alive_.size(), 0);
    initial.clear();
    final.clear();
    for (std::size_t p = 0; p < place_alive_.size(); ++p) {
      if (!place_alive_[p]) continue;
      if (initial_[p] > kTokenLimit || final_[p] > kTokenLimit) return false;
      renumber[p] = net.add_place();
      initial.push_back(static_cast<int>(initial_[p]));
      final.push_back(static_cast<int>(final_[p]));
    }
    net.set_activities({"visible"});
    for (std::size_t t = 0; t < transition_alive_.size(); ++t) {
      if (!transition_alive_[t]) continue;
      std::vector<std::size_t> inputs, outputs;
      for (const auto& [place, k] : pre_[t]) inputs.insert(inputs.end(), std::size_t(k), renumber[place]);
      for (const auto& [place, k] : post_[t]) outputs.insert(outputs.end(), std::size_t(k), renumber[place]);
      net.add_transition(visible_[t] ? std::optional<std::size_t>(0) : std::nullopt,
                         std::move(inputs), std::move(outputs));
    }
    return true;
  }

 private:
  using Arcs = std::vector<std::pair<std::uint32_t, std::int64_t>>;  // sorted by id
  static constexpr std::size_t kLimit = std::size_t{1} << 30;
  // Agglomeration keeps arc multiplicities at most kArcLimit and the token
  // counts it moves at most kTokenLimit, so a capped search over the reduced
  // net stays far from int (the search's token type) as one over the
  // original does.
  static constexpr std::int64_t kArcLimit = 64;
  static constexpr std::int64_t kTokenLimit = std::int64_t{1} << 20;

  static std::int64_t count(const Arcs& arcs, std::uint32_t id) {
    const auto it = std::lower_bound(arcs.begin(), arcs.end(), std::make_pair(id, std::int64_t{0}));
    return it != arcs.end() && it->first == id ? it->second : 0;
  }
  static void add(Arcs& arcs, std::uint32_t id, std::int64_t k) {
    const auto it = std::lower_bound(arcs.begin(), arcs.end(), std::make_pair(id, std::int64_t{0}));
    if (it != arcs.end() && it->first == id) it->second += k;
    else arcs.insert(it, {id, k});
  }
  static void erase(Arcs& arcs, std::uint32_t id) {
    const auto it = std::lower_bound(arcs.begin(), arcs.end(), std::make_pair(id, std::int64_t{0}));
    if (it != arcs.end() && it->first == id) arcs.erase(it);
  }

  void remove_transition(std::uint32_t t) {
    for (const auto& [place, k] : pre_[t]) erase(consumers_[place], t);
    for (const auto& [place, k] : post_[t]) erase(producers_[place], t);
    arcs_ -= pre_[t].size() + post_[t].size();
    pre_[t].clear();
    post_[t].clear();
    transition_alive_[t] = 0;
  }
  void remove_place(std::uint32_t p) {
    for (const auto& [t, k] : consumers_[p]) erase(pre_[t], p);
    for (const auto& [t, k] : producers_[p]) erase(post_[t], p);
    arcs_ -= consumers_[p].size() + producers_[p].size();
    consumers_[p].clear();
    producers_[p].clear();
    place_alive_[p] = 0;
  }

  bool drop_transitions() {
    bool changed = false;
    std::map<std::pair<Arcs, Arcs>, std::uint32_t> seen;
    for (std::uint32_t t = 0; t < transition_alive_.size(); ++t) {
      if (!transition_alive_[t]) continue;
      bool drop = pre_[t] == post_[t];
      for (const auto& [place, k] : pre_[t])
        drop = drop || (producers_[place].empty() && initial_[place] < k);
      if (!drop) {
        const auto [it, added] = seen.emplace(std::make_pair(pre_[t], post_[t]), t);
        drop = !added;
      }
      if (drop) {
        remove_transition(t);
        changed = true;
      }
    }
    return changed;
  }

  bool drop_places() {
    bool changed = false;
    std::map<std::tuple<Arcs, Arcs, std::int64_t, std::int64_t>, std::uint32_t> seen;
    for (std::uint32_t p = 0; p < place_alive_.size(); ++p) {
      if (!place_alive_[p]) continue;
      bool drop = initial_[p] == final_[p] && consumers_[p] == producers_[p] &&
                  std::all_of(consumers_[p].begin(), consumers_[p].end(),
                              [&](const auto& arc) { return arc.second <= initial_[p]; });
      if (!drop) {
        const auto [it, added] = seen.emplace(
            std::make_tuple(producers_[p], consumers_[p], initial_[p], final_[p]), p);
        drop = !added;
      }
      if (drop) {
        remove_place(p);
        changed = true;
      }
    }
    return changed;
  }

  bool post_agglomerate(std::uint32_t t) {
    if (pre_[t].size() != 1 || pre_[t][0].second != 1) return false;
    const auto p = pre_[t][0].first;
    if (consumers_[p].size() != 1 || final_[p] != 0 || count(post_[t], p) != 0) return false;
    const Arcs moved = post_[t];
    const Arcs producers = producers_[p];
    if (initial_[p] > kTokenLimit) return false;
    for (const auto& [r, c] : moved)
      if (c > kArcLimit || initial_[r] > kTokenLimit ||
          initial_[r] + initial_[p] * c > kTokenLimit)
        return false;
    if (arcs_ + producers.size() * moved.size() > arc_budget_) return false;
    for (const auto& [u, k] : producers)
      for (const auto& [r, c] : moved)
        if (k > kArcLimit || count(post_[u], r) + k * c > kArcLimit) return false;
    const bool visible = visible_[t] != 0;
    const std::int64_t tokens = initial_[p];
    remove_transition(t);
    for (const auto& [u, k] : producers) {
      erase(post_[u], p);
      --arcs_;
      for (const auto& [r, c] : moved) {
        if (count(post_[u], r) == 0) ++arcs_;
        add(post_[u], r, k * c);
        add(producers_[r], u, k * c);
      }
      if (visible) visible_[u] = 1;
    }
    for (const auto& [r, c] : moved) initial_[r] += tokens * c;
    producers_[p].clear();
    initial_[p] = 0;
    place_alive_[p] = 0;
    return true;
  }

  bool pre_agglomerate(std::uint32_t t) {
    if (post_[t].size() != 1 || post_[t][0].second != 1) return false;
    const auto q = post_[t][0].first;
    if (producers_[q].size() != 1 || initial_[q] != 0 || final_[q] != 0 ||
        count(pre_[t], q) != 0)
      return false;
    const Arcs taken = pre_[t];
    const Arcs consumers = consumers_[q];
    if (arcs_ + consumers.size() * taken.size() > arc_budget_) return false;
    for (const auto& [v, k] : consumers)
      for (const auto& [r, c] : taken)
        if (k > kArcLimit || c > kArcLimit || count(pre_[v], r) + k * c > kArcLimit)
          return false;
    const bool visible = visible_[t] != 0;
    remove_transition(t);
    for (const auto& [v, k] : consumers) {
      erase(pre_[v], q);
      --arcs_;
      for (const auto& [r, c] : taken) {
        if (count(pre_[v], r) == 0) ++arcs_;
        add(pre_[v], r, k * c);
        add(consumers_[r], v, k * c);
      }
      if (visible) visible_[v] = 1;
    }
    consumers_[q].clear();
    place_alive_[q] = 0;
    return true;
  }

  std::vector<Arcs> pre_, post_;             // by transition: places
  std::vector<Arcs> consumers_, producers_;  // by place: transitions
  std::vector<char> visible_, transition_alive_, place_alive_;
  std::vector<std::int64_t> initial_, final_;
  std::size_t arcs_ = 0, arc_budget_ = 0;
};

// The reachability-only search: the structural reduction first, then the
// Aligner's capped searches on the reduced net with a quarter of the state
// budget (only when at most two thirds of the places and transitions are
// left), and the unreduced searches with the full budget only when those
// stopped at it (so every verdict the unreduced searches decide is still
// decided, and a net neither decides costs at most about a quarter more).
FinalMarkingReachability search_final_marking_reachability(const PetriNet& net,
                                                           const AlignmentOptions& options) {
  // Resolved as the Aligner does (and so throwing the same errors first).
  const auto initial = resolve_workflow_marking(net, options.initial_marking, true);
  const auto final = resolve_workflow_marking(net, options.final_marking, false);
  ReachabilityReduction reduction;
  if (reduction.load(net, initial, final)) {
    const auto outcome = reduction.reduce(options.cancelled);
    if (outcome == ReachabilityReduction::Outcome::kTrivial)
      return FinalMarkingReachability::kReachable;
    PetriNet reduced;
    AlignmentOptions reduced_options = options;
    reduced_options.max_states =
        std::max<std::size_t>(1, (options.max_states == 0 ? kAlignSearchCap : options.max_states) / 4);
    // A residue keeping more than two thirds of the net would likely stop
    // where the unreduced search does (the undecided Heuristics nets of the
    // review AA2 fuzz kept 70-80%), so that search runs directly.
    const bool shrank = reduction.size() * 3 <= (net.place_count() + net.transitions().size()) * 2;
    if (outcome == ReachabilityReduction::Outcome::kReduced && shrank &&
        reduction.build(reduced, reduced_options.initial_marking,
                        reduced_options.final_marking)) {
      std::optional<FinalMarkingReachability> verdict;
      try {
        const Aligner aligner(reduced, reduced_options,
                              std::chrono::steady_clock::time_point::max(),
                              /*reachability_only=*/true);
        if (aligner.reachable()) verdict = FinalMarkingReachability::kReachable;
        else if (!aligner.reachability_exhausted()) verdict = FinalMarkingReachability::kUnreachable;
      } catch (const std::overflow_error&) {
        // Left to the unreduced search, which reports its own outcome.
      }
      if (verdict) return *verdict;
      // Cancelled: the callers report it.
      if (options.cancelled && options.cancelled()) return FinalMarkingReachability::kExhausted;
    }
  }
  const Aligner aligner(net, options, std::chrono::steady_clock::time_point::max(),
                        /*reachability_only=*/true);
  if (aligner.reachable()) return FinalMarkingReachability::kReachable;
  return aligner.reachability_exhausted() ? FinalMarkingReachability::kExhausted
                                          : FinalMarkingReachability::kUnreachable;
}

}  // namespace

bool final_marking_reachable(const PetriNet& net) {
  return final_marking_reachable(net, {}, {});
}

bool final_marking_reachable(const PetriNet& net, const std::function<bool()>& cancelled) {
  AlignmentOptions options;
  options.cancelled = cancelled;
  const bool reachable = search_final_marking_reachability(net, options) ==
                         FinalMarkingReachability::kReachable;
  // A cancelled search reports the final marking unreachable.
  if (!reachable) throw_if_cancelled(cancelled, "reachability search was cancelled");
  return reachable;
}

FinalMarkingReachability final_marking_reachability(
    const PetriNet& net, const std::function<bool()>& cancelled) {
  AlignmentOptions options;
  options.cancelled = cancelled;
  const auto verdict = search_final_marking_reachability(net, options);
  if (verdict == FinalMarkingReachability::kReachable) return verdict;
  throw_if_cancelled(cancelled, "reachability search was cancelled");
  return verdict;
}

FinalMarkingReachability final_marking_reachability(
    const PetriNet& net, const std::vector<int>& initial_marking,
    const std::vector<int>& final_marking) {
  AlignmentOptions options;
  options.initial_marking = initial_marking;
  options.final_marking = final_marking;
  return search_final_marking_reachability(net, options);
}

bool final_marking_reachable(const PetriNet& net,
                             const std::vector<int>& initial_marking,
                             const std::vector<int>& final_marking) {
  AlignmentOptions options;
  options.initial_marking = initial_marking;
  options.final_marking = final_marking;
  return search_final_marking_reachability(net, options) ==
         FinalMarkingReachability::kReachable;
}

}  // namespace minera
