#include "minera/streaming_tbr.hpp"

#include "minera/streaming_dfg.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const unsigned char character : value) {
    switch (character) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (character < 0x20U) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<unsigned int>(character) << std::dec
              << std::setfill(' ');
        } else {
          out << static_cast<char>(character);
        }
    }
  }
  out << '"';
}

struct VectorLess {
  bool operator()(const std::vector<int>& left,
                  const std::vector<int>& right) const noexcept {
    return left < right;
  }
};

}  // namespace

StreamingTokenReplay::StreamingTokenReplay(PetriNet net,
                                             StreamingTbrOptions options)
    : net_(std::move(net)), options_(std::move(options)) {
  const bool explicit_markings =
      !options_.initial_marking.empty() && !options_.final_marking.empty();
  if (net_.place_count() == 0 ||
      (!explicit_markings && (net_.source() >= net_.place_count() ||
                              net_.sink() >= net_.place_count())))
    throw std::invalid_argument("streaming token replay net has invalid boundaries");
  if (options_.activity_attribute.empty() || options_.case_id_attribute.empty())
    throw std::invalid_argument(
        "streaming token replay attribute names must not be empty");
  for (std::size_t transition = 0; transition < net_.transitions().size();
       ++transition) {
    const auto& current = net_.transitions()[transition];
    for (const auto place : current.inputs)
      if (place >= net_.place_count())
        throw std::invalid_argument("streaming token replay input place is invalid");
    for (const auto place : current.outputs)
      if (place >= net_.place_count())
        throw std::invalid_argument("streaming token replay output place is invalid");
    if (current.silent()) {
      silent_transitions_.push_back(transition);
    } else {
      if (*current.activity >= net_.activities().size())
        throw std::invalid_argument(
            "streaming token replay activity index is invalid");
      transitions_by_activity_[net_.activities()[*current.activity]].push_back(
          transition);
    }
  }
  const auto places = net_.place_count();
  const auto marking_or = [&](const std::vector<int>& given, std::size_t place) {
    if (given.empty()) {
      Marking marking(places, 0);
      marking[place] = 1;
      return marking;
    }
    if (given.size() != places)
      throw std::invalid_argument("streaming token replay marking size is invalid");
    for (const auto tokens : given)
      if (tokens < 0) throw std::invalid_argument("streaming token replay marking is negative");
    return given;
  };
  initial_ = marking_or(options_.initial_marking, net_.source());
  final_ = marking_or(options_.final_marking, net_.sink());
  for (std::size_t place = 0; place < places; ++place)
    if (initial_[place] > 0) initial_order_.push_back(place);
  silent_paths_.assign(places, std::vector<std::optional<std::vector<std::size_t>>>(places));
  for (std::size_t from = 0; from < places; ++from) {
    auto& paths = silent_paths_[from];
    paths[from] = std::vector<std::size_t>{};
    std::queue<std::size_t> pending;
    pending.push(from);
    while (!pending.empty()) {
      const auto place = pending.front();
      pending.pop();
      for (const auto silent : silent_transitions_) {
        const auto& inputs = net_.transitions()[silent].inputs;
        if (std::find(inputs.begin(), inputs.end(), place) == inputs.end()) continue;
        for (const auto next : net_.transitions()[silent].outputs) {
          if (paths[next]) continue;
          auto path = *paths[place];
          path.push_back(silent);
          paths[next] = std::move(path);
          pending.push(next);
        }
      }
    }
  }
}

void StreamingTokenReplay::ordered_fire(CaseState& state,
                                        std::size_t transition) const {
  const auto& current = net_.transitions()[transition];
  for (const auto place : current.inputs) {
    if (--state.marking[place] == 0)
      state.order.erase(std::find(state.order.begin(), state.order.end(), place));
  }
  for (const auto place : current.outputs)
    if (state.marking[place]++ == 0) state.order.push_back(place);
}

std::optional<StreamingTokenReplay::CaseState>
StreamingTokenReplay::enable_with_invisibles(const CaseState& state,
                                             std::size_t transition) const {
  const auto& inputs = net_.transitions()[transition].inputs;
  std::vector<int> required(net_.place_count(), 0);
  for (const auto place : inputs) ++required[place];
  CaseState copy = state;
  std::set<std::size_t> used;
  std::set<std::size_t> visited;
  // MAXIMUM_ITERATIONS_INVISIBLES bounds the reference loop: each iteration
  // either finds the transition enabled or fires one silent path towards one
  // missing input place. A chain of any length is one iteration, and the
  // enabling check itself takes one, so a single path needs a limit of 2.
  std::size_t iterations = 0;
  for (const auto place : inputs) {
    if (!visited.insert(place).second || copy.marking[place] >= required[place]) continue;
    if (iterations++ >= options_.maximum_iterations_invisibles) return std::nullopt;
    const std::vector<std::size_t>* best = nullptr;
    std::size_t best_source = 0;
    for (const auto source : copy.order) {
      if (required[source] > 0 || used.contains(source)) continue;
      const auto& path = silent_paths_[source][place];
      if (path && (!best || path->size() < best->size())) best = &*path, best_source = source;
    }
    if (!best) return std::nullopt;
    for (const auto silent : *best) {
      if (!enabled(copy.marking, silent)) return std::nullopt;
      ordered_fire(copy, silent);
    }
    used.insert(best_source);
  }
  if (iterations >= options_.maximum_iterations_invisibles || !enabled(copy.marking, transition)) return std::nullopt;
  return copy;
}

std::string StreamingTokenReplay::variant_name() const noexcept {
  return options_.variant == StreamingTbrVariant::kBackwards ? "BACKWARDS"
                                                               : "FORWARD";
}

bool StreamingTokenReplay::enabled(const Marking& marking,
                                   std::size_t transition) const {
  const auto& current = net_.transitions()[transition];
  std::vector<int> required(marking.size(), 0);
  for (const auto place : current.inputs) ++required[place];
  for (std::size_t place = 0; place < required.size(); ++place)
    if (marking[place] < required[place]) return false;
  return true;
}

std::uint64_t StreamingTokenReplay::fire(Marking& marking,
                                         std::size_t transition) const {
  const auto& current = net_.transitions()[transition];
  std::vector<int> required(marking.size(), 0);
  for (const auto place : current.inputs) ++required[place];
  std::uint64_t missing = 0;
  for (std::size_t place = 0; place < required.size(); ++place) {
    if (marking[place] < required[place]) {
      missing += static_cast<std::uint64_t>(required[place] - marking[place]);
      marking[place] = 0;
    } else {
      marking[place] -= required[place];
    }
  }
  for (const auto place : current.outputs) ++marking[place];
  return missing;
}

std::optional<StreamingTokenReplay::Route>
StreamingTokenReplay::route_to_visible(
    const Marking& marking,
    const std::vector<std::size_t>& visible_transitions) const {
  for (const auto transition : visible_transitions)
    if (enabled(marking, transition)) return Route{marking, {}};
  struct Node {
    Marking marking;
    std::vector<std::size_t> path;
  };
  std::queue<Node> pending;
  std::set<Marking, VectorLess> visited;
  pending.push({marking, {}});
  visited.insert(marking);
  std::size_t expanded = 0;
  while (!pending.empty() && expanded++ < options_.maximum_iterations_invisibles) {
    auto current = std::move(pending.front());
    pending.pop();
    for (const auto transition : silent_transitions_) {
      if (!enabled(current.marking, transition)) continue;
      auto next_marking = current.marking;
      fire(next_marking, transition);
      auto path = current.path;
      path.push_back(transition);
      for (const auto visible : visible_transitions)
        if (enabled(next_marking, visible))
          return Route{std::move(next_marking), std::move(path)};
      if (visited.insert(next_marking).second)
        pending.push({std::move(next_marking), std::move(path)});
    }
  }
  return std::nullopt;
}

std::optional<StreamingTokenReplay::Route>
StreamingTokenReplay::route_to_final(const Marking& marking) const {
  const Marking& target = final_;
  if (marking == target) return Route{marking, {}};
  struct Node {
    Marking marking;
    std::vector<std::size_t> path;
  };
  std::queue<Node> pending;
  std::set<Marking, VectorLess> visited;
  pending.push({marking, {}});
  visited.insert(marking);
  std::size_t expanded = 0;
  while (!pending.empty() && expanded++ < options_.maximum_iterations_invisibles) {
    auto current = std::move(pending.front());
    pending.pop();
    for (const auto transition : silent_transitions_) {
      if (!enabled(current.marking, transition)) continue;
      auto next_marking = current.marking;
      fire(next_marking, transition);
      auto path = current.path;
      path.push_back(transition);
      if (next_marking == target)
        return Route{std::move(next_marking), std::move(path)};
      if (visited.insert(next_marking).second)
        pending.push({std::move(next_marking), std::move(path)});
    }
  }
  return std::nullopt;
}

std::uint64_t StreamingTokenReplay::final_missing(
    const Marking& marking) const {
  std::uint64_t missing = 0;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    const int target = final_[place];
    if (marking[place] < target)
      missing += static_cast<std::uint64_t>(target - marking[place]);
  }
  return missing;
}

std::uint64_t StreamingTokenReplay::final_remaining(
    const Marking& marking) const {
  std::uint64_t remaining = 0;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    const int target = final_[place];
    if (marking[place] > target)
      remaining += static_cast<std::uint64_t>(marking[place] - target);
  }
  return remaining;
}

StreamingTbrEventResult StreamingTokenReplay::receive(
    std::string_view case_id, std::string_view activity) {
  // Empty case ids and activities are ordinary values, as in the reference.
  std::lock_guard<std::mutex> lock(mutex_);
  const auto transitions_it = transitions_by_activity_.find(
      std::string(activity));
  if (transitions_it == transitions_by_activity_.end()) {
    // Unknown labels are ignored by both online variations; the backwards
    // variant explicitly documents this behavior and the forward monitor does
    // not create a case for an activity absent from the model.
    return {std::string(case_id), std::string(activity), 0, 0, false, true};
  }
  const std::string case_key(case_id);
  auto [it, inserted] = cases_.try_emplace(case_key);
  if (inserted) it->second.marking = initial_, it->second.order = initial_order_;
  auto& state = it->second;
  if (options_.stop_immediately_unfit && state.hard_unfit)
    return {case_key, std::string(activity), 0, 0, false, false};
  std::uint64_t missing_added = 0;
  bool accepted = true;
  const auto& visible = transitions_it->second;
  if (options_.variant == StreamingTbrVariant::kBackwards) {
    bool fired = false;
    for (const auto transition : visible) {
      if (enabled(state.marking, transition)) {
        missing_added += fire(state.marking, transition);
        fired = true;
      }
    }
    if (!fired) {
      if (visible.size() != 1) {
        state.hard_unfit = true;
        accepted = false;
      } else if (const auto route = route_to_visible(state.marking, visible)) {
        state.marking = route->marking;
        const auto visible_transition = visible.front();
        for (const auto silent : route->silent_transitions)
          (void)silent;
        missing_added += fire(state.marking, visible_transition);
      } else {
        missing_added += fire(state.marking, visible.front());
        state.hard_unfit = true;
        if (options_.stop_immediately_unfit) accepted = false;
      }
    }
  } else {
    bool fired = false;
    for (const auto transition : visible)
      if (enabled(state.marking, transition)) {
        ordered_fire(state, transition);
        fired = true;
        break;
      }
    for (std::size_t i = 0; !fired && i < visible.size(); ++i)
      if (auto next = enable_with_invisibles(state, visible[i])) {
        next->events = state.events;
        next->missing = state.missing;
        state = std::move(*next);
        ordered_fire(state, visible[i]);
        fired = true;
      }
    if (!fired) {
      // Missing tokens are inserted for the first transition, counted per
      // input place in arc order. The reference fails on reaching an input
      // place that still holds a token: the count so far stays and the
      // marking is left unchanged.
      const auto transition = visible.front();
      const auto& inputs = net_.transitions()[transition].inputs;
      std::vector<std::size_t> places;
      if (transition < options_.input_place_order.size() &&
          !options_.input_place_order[transition].empty())
        places = options_.input_place_order[transition];
      else
        for (const auto place : inputs)
          if (std::find(places.begin(), places.end(), place) == places.end()) places.push_back(place);
      for (const auto place : places) {
        if (state.marking[place] > 0) {
          state.missing += missing_added;
          total_missing_ += missing_added;
          return {case_key, std::string(activity), 0, missing_added, false, state.missing == 0};
        }
        missing_added += static_cast<std::uint64_t>(std::count(inputs.begin(), inputs.end(), place));
      }
      for (const auto place : net_.transitions()[transition].outputs)
        if (state.marking[place]++ == 0) state.order.push_back(place);
    }
  }
  ++state.events;
  state.missing += missing_added;
  ++total_events_processed_;
  total_missing_ += missing_added;
  return {case_key, std::string(activity), total_events_processed_,
          missing_added, accepted, !state.hard_unfit && state.missing == 0};
}

StreamingTbrEventResult StreamingTokenReplay::receive(
    const Event& event, std::string_view case_id) {
  // The reference skips an event whose activity is missing or None.
  const auto activity = streaming_event_present_value(event, options_.activity_attribute);
  if (!activity) return {};
  const std::string resolved_case =
      case_id.empty() ? event.case_id : std::string(case_id);
  return receive(resolved_case, *activity);
}

std::optional<StreamingTbrTerminationResult>
StreamingTokenReplay::terminate_case(const std::string& case_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = cases_.find(case_id);
  if (it == cases_.end()) return std::nullopt;
  auto state = std::move(it->second);
  auto marking = state.marking;
  // The reference compares the marking with the final one as it stands.
  if (options_.variant == StreamingTbrVariant::kBackwards)
    if (const auto route = route_to_final(marking)) marking = route->marking;
  const auto missing = state.missing + final_missing(marking);
  const auto remaining = final_remaining(marking);
  total_missing_ += final_missing(marking);
  const bool fit = !state.hard_unfit && missing == 0 && remaining == 0;
  cases_.erase(it);
  return StreamingTbrTerminationResult{std::move(marking), missing, remaining,
                                       fit};
}

void StreamingTokenReplay::terminate_all() {
  std::vector<std::string> cases;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [case_id, state] : cases_) {
      (void)state;
      cases.push_back(case_id);
    }
  }
  for (const auto& case_id : cases) (void)terminate_case(case_id);
}

void StreamingTokenReplay::reset_case(const std::string& case_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  cases_.erase(case_id);
}

void StreamingTokenReplay::reset_all() {
  std::lock_guard<std::mutex> lock(mutex_);
  cases_.clear();
  total_events_processed_ = 0;
  total_missing_ = 0;
}

StreamingTbrSnapshot StreamingTokenReplay::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  StreamingTbrSnapshot result;
  result.variant = variant_name();
  result.total_events_processed = total_events_processed_;
  result.total_missing = total_missing_;
  for (const auto& [case_id, state] : cases_) {
    result.cases.emplace(
        case_id, StreamingTbrCaseSnapshot{state.events, state.missing, 0,
                                          !state.hard_unfit && state.missing == 0,
                                          state.marking});
  }
  return result;
}

std::string serialize_streaming_tbr_json(
    const StreamingTbrSnapshot& snapshot) {
  std::ostringstream out;
  out << "{\"family\":\"STREAMING_TBR\",\"variant\":";
  write_json_string(out, snapshot.variant);
  out << ",\"total_events_processed\":" << snapshot.total_events_processed
      << ",\"total_missing\":" << snapshot.total_missing
      << ",\"cases\":{";
  bool first = true;
  for (const auto& [case_id, state] : snapshot.cases) {
    if (!first) out << ',';
    first = false;
    write_json_string(out, case_id);
    out << ":{\"events\":" << state.events
        << ",\"missing\":" << state.missing
        << ",\"remaining\":" << state.remaining
        << ",\"is_fit\":" << (state.is_fit ? "true" : "false")
        << ",\"marking\":[";
    for (std::size_t i = 0; i < state.marking.size(); ++i) {
      if (i != 0) out << ',';
      out << state.marking[i];
    }
    out << "]}";
  }
  out << "}}";
  return out.str();
}

}  // namespace minera
