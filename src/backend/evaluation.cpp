#include "minera/evaluation.hpp"
#include "minera/log_conversion.hpp"
#include "minera/pnml.hpp"
#include "cyclomatic_unicode.hpp"
#include "root_cause_unicode.hpp"
#include "evaluation_internal.hpp"
#include "petri_replay_internal.hpp"
#include "reference_activity_selector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace minera {
namespace {

using Marking = std::vector<int>;
struct MarkingHash { std::size_t operator()(const Marking& m) const noexcept {
  std::size_t h = 1469598103934665603ull; for (int x : m) h = (h ^ static_cast<unsigned>(x)) * 1099511628211ull; return h;
} };

Marking initial(const PetriNet& n, const SimplicityOptions& o) {
  if (!o.initial_marking.empty()) return o.initial_marking;
  Marking m(n.place_count(), 0); if (n.source()<m.size()) m[n.source()]=1;
  return m;
}
bool enabled(const PetriTransition& t, const Marking& m) {
  return petri_transition_enabled(t, m);
}
Marking fire(const PetriTransition& t, Marking m) {
  fire_petri_transition(t, m); return m;
}
bool silent_closure(const PetriNet& n, const Marking& start,
                    std::size_t cap, std::vector<Marking>& out) {
  out.clear();
  if (cap == 0) return false;
  out.push_back(start);
  std::queue<std::size_t> q;
  q.push(0);
  std::unordered_set<Marking, MarkingHash> seen;
  seen.insert(start);
  while (!q.empty()) {
    const auto i = q.front();
    q.pop();
    for (const auto& t : n.transitions()) {
      if (!t.silent() || !enabled(t, out[i])) continue;
      auto next = fire(t, out[i]);
      if (seen.insert(next).second) {
        if (out.size() >= cap) return false;
        out.push_back(std::move(next));
        q.push(out.size() - 1);
      }
    }
  }
  return true;
}

std::vector<Marking> closure(const PetriNet& n, Marking start,
                             std::size_t cap = 100000) {
  std::vector<Marking> out;
  silent_closure(n, start, cap, out);
  return out;
}

// Labels the pinned evaluator observes as eventually enabled from a marking.
// It walks a worklist of transitions in name order. Every silent firing
// appends the transitions enabled afterwards and moves each of them to that
// newest marking, even while an earlier entry is still waiting. A transition
// is handled once per marking, so a silent transition used on one branch can
// be unavailable on another, and visible labels behind it are not reported.
std::vector<std::size_t> transitions_in_name_order(const PetriNet& n,
                                                   const std::vector<std::string>& names) {
  std::vector<std::size_t> order(n.transitions().size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  if (names.size() == order.size())
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return names[a] < names[b]; });
  return order;
}

bool eventually_enabled_labels(const PetriNet& n, const Marking& m,
                               const std::vector<std::size_t>& order, std::size_t cap,
                               bool use_task_ids, std::set<std::string>& labels) {
  labels.clear();
  if (cap == 0) return false;
  std::vector<std::size_t> work;
  std::vector<Marking> at(n.transitions().size());
  for (const auto i : order) {
    if (!enabled(n.transitions()[i], m)) continue;
    work.push_back(i);
    at[i] = m;
  }
  std::set<std::pair<std::size_t, Marking>> handled;
  for (std::size_t k = 0; k < work.size(); ++k) {
    const auto i = work[k];
    if (!handled.emplace(i, at[i]).second) continue;
    const auto& t = n.transitions()[i];
    if (!t.silent()) {
      labels.insert(use_task_ids ? "t" + std::to_string(i) : n.activities()[*t.activity]);
      continue;
    }
    const auto next = fire(t, at[i]);
    for (const auto j : order) {
      if (!enabled(n.transitions()[j], next)) continue;
      if (work.size() >= cap) return false;
      work.push_back(j);
      at[j] = next;
    }
  }
  return true;
}

double levenshtein(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  std::vector<double> d(b.size()+1); std::iota(d.begin(), d.end(), 0.0);
  for (std::size_t i=1;i<=a.size();++i) { double prev=d[0]; d[0]=double(i);
    for (std::size_t j=1;j<=b.size();++j) { double x=d[j]; d[j]=std::min({d[j]+1,d[j-1]+1,prev+(a[i-1]==b[j-1]?0:1)}); prev=x; }
  }
  return d.back() / double(std::max(a.size(),b.size()));
}

} // namespace

double normalized_levenshtein_distance(const std::vector<std::string>& a,const std::vector<std::string>& b) {
  if (a.empty() && b.empty()) return 0.0; return levenshtein(a,b);
}

ReplayFitnessResult evaluate_replay_fitness(const EventLog& log, const PetriNet& net, ReplayFitnessVariant variant) {
  return evaluate_replay_fitness(log, net, variant, {});
}

namespace {

std::string selected_evaluation_activity(
    const Event& event, std::string_view activity_attribute) {
  return reference_selected_activity(event, activity_attribute, "evaluation");
}

void validate_evaluation_selector(std::string_view selector,
                                  std::string_view name) {
  if (selector.empty())
    throw std::invalid_argument("evaluation: " + std::string(name) +
                                " selector must not be empty");
}

int evaluation_activity_selector_priority(const EventAttribute& field,
                                           std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool alias = selector == "concept:name" || selector == "activity";
  return alias && (field.key == "concept:name" || field.key == "activity")
             ? 1
             : 0;
}

int evaluation_case_selector_priority(const EventAttribute& field,
                                      std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool alias = selector == "case:concept:name" ||
                     selector == "case:id" || selector == "case_id" ||
                     selector == "case";
  return alias && (field.key == "case:concept:name" ||
                   field.key == "case:id" || field.key == "case_id" ||
                   field.key == "case")
             ? 1
             : 0;
}

EventLog project_evaluation_activity(const EventLog& log,
                                     std::string_view activity_attribute);

template <typename Priority>
std::optional<std::size_t> select_evaluation_selector_field(
    const EventRecord& record, std::string_view selector,
    std::string_view name, Priority priority) {
  int best_priority = 0;
  std::optional<std::size_t> selected;
  std::size_t matches = 0;
  for (std::size_t index = 0; index < record.fields.size(); ++index) {
    const auto& field = record.fields[index];
    const int current = priority(field, selector);
    if (current == 0) continue;
    if (current > best_priority) {
      best_priority = current;
      selected = index;
      matches = 1;
    } else if (current == best_priority) {
      ++matches;
    }
  }
  if (!selected) return std::nullopt;
  if (matches > 1) {
    throw std::invalid_argument("evaluation: " + std::string(name) +
                                " selector is ambiguous: " +
                                std::string(selector));
  }
  return selected;
}

template <typename Priority>
void validate_evaluation_selector_fields(const EventRecord& record,
                                         std::string_view selector,
                                         std::string_view name,
                                         Priority priority) {
  (void)select_evaluation_selector_field(record, selector, name, priority);
}

void validate_evaluation_selectors(const EventStream& stream,
                                   std::string_view activity_attribute,
                                   std::string_view case_id_attribute) {
  for (const auto& record : stream.events) {
    validate_evaluation_selector_fields(
        record, activity_attribute, "activity",
        evaluation_activity_selector_priority);
    validate_evaluation_selector_fields(
        record, case_id_attribute, "case-id", evaluation_case_selector_priority);
  }
}

void project_evaluation_stream(EventStream& stream,
                               std::string_view activity_attribute,
                               std::string_view case_id_attribute) {
  for (auto& record : stream.events) {
    const auto activity_index = select_evaluation_selector_field(
        record, activity_attribute, "activity",
        evaluation_activity_selector_priority);
    const auto case_index = select_evaluation_selector_field(
        record, case_id_attribute, "case-id",
        evaluation_case_selector_priority);
    const auto selected_activity =
        activity_index ? std::optional<EventAttribute>(record.fields[*activity_index])
                       : std::nullopt;
    const auto selected_case =
        case_index ? std::optional<EventAttribute>(record.fields[*case_index])
                   : std::nullopt;
    for (auto& field : record.fields) {
      // Clear stale roles before installing authoritative projections for the
      // two requested selectors.
      field.first_class_role = FirstClassAttributeRole::kNone;
      field.first_class_state = FirstClassAttributeState::kMissing;
      field.first_class_source_key.clear();
    }
    const auto append_projection = [&](EventAttribute field,
                                       FirstClassAttributeRole role,
                                       std::string_view source_key) {
      const auto state = field.first_class_role == FirstClassAttributeRole::kNone
                             ? (field.is_null
                                    ? FirstClassAttributeState::kNull
                                    : FirstClassAttributeState::kPresent)
                             : field.first_class_state;
      field.first_class_role = role;
      field.first_class_state = state;
      field.first_class_source_key = std::string(source_key);
      record.fields.push_back(std::move(field));
    };
    if (selected_case) {
      append_projection(*selected_case, FirstClassAttributeRole::kCaseId,
                        case_id_attribute);
    }
    if (selected_activity) {
      append_projection(*selected_activity,
                        FirstClassAttributeRole::kActivity,
                        activity_attribute);
    }
  }
}

EventLog evaluation_event_log(const EventDataView& data,
                              std::string_view activity_attribute,
                              std::string_view case_id_attribute,
                              std::optional<std::size_t> max_rows) {
  const auto activity = activity_attribute.empty() ? "concept:name"
                                                    : activity_attribute;
  validate_evaluation_selector(activity, "activity");
  validate_evaluation_selector(case_id_attribute, "case-id");
  LogConversionOptions conversion;
  conversion.case_id_key = std::string(case_id_attribute);
  conversion.max_rows_to_materialize = max_rows;

  if (data.kind() == EventDataKind::kEventLog) {
    const auto* log = data.log();
    if (max_rows && log->event_count() > *max_rows)
      throw std::length_error("evaluation: row materialization cap exceeded");
    return project_evaluation_activity(*log, activity);
  }

  auto stream = event_data_to_event_stream(data, conversion);
  if (max_rows && stream.events.size() > *max_rows)
    throw std::length_error("evaluation: row materialization cap exceeded");
  // Conversion already preserves first-class projections and generic fields.
  // Clear stale roles and project the requested glue key before grouping so an
  // older case projection cannot override the caller's selector.
  validate_evaluation_selectors(stream, activity, case_id_attribute);
  project_evaluation_stream(stream, activity, case_id_attribute);
  return project_evaluation_activity(
      event_stream_to_event_log(stream, conversion), activity);
}

void enforce_evaluation_event_log_cap(
    const EventLog& log, std::optional<std::size_t> max_rows) {
  if (max_rows && log.event_count() > *max_rows)
    throw std::length_error("evaluation: row materialization cap exceeded");
}

EventLog project_evaluation_activity(
    const EventLog& log, std::string_view activity_attribute) {
  EventLog projected = log;
  for (std::size_t trace = 0; trace < log.traces.size(); ++trace) {
    for (std::size_t event = 0; event < log.traces[trace].events.size();
         ++event) {
      const auto& source = log.traces[trace].events[event];
      auto& target = projected.traces[trace].events[event];
      const auto selected =
          selected_evaluation_activity(source, activity_attribute);
      set_event_activity_context(
          source, selected, target);
      if (!activity_attribute.empty() && activity_attribute != "concept:name" &&
          activity_attribute != "activity") {
        // The lower-level replay engines resolve custom selectors through the
        // generic attribute collection. Keep a synchronized string view in
        // addition to the first-class projection used by adapters.
        auto found = std::find_if(
            target.attributes.begin(), target.attributes.end(),
            [&](const auto& attribute) {
              return attribute.key == activity_attribute;
            });
        if (found == target.attributes.end()) {
          target.attributes.push_back(
              {std::string(activity_attribute), selected, AttributeKind::kString});
        } else {
          found->value = selected;
          found->is_null = false;
        }
      }
    }
  }
  return projected;
}

std::string validate_evaluation_markings(const PetriNet& net,
                                         const Marking& initial_marking,
                                         const Marking& final_marking) {
  if ((!initial_marking.empty() &&
       initial_marking.size() != net.place_count()) ||
      (!final_marking.empty() &&
       final_marking.size() != net.place_count())) {
    return "marking dimension does not match net";
  }
  const auto has_negative = [](const Marking& marking) {
    return std::any_of(marking.begin(), marking.end(),
                       [](int tokens) { return tokens < 0; });
  };
  if (has_negative(initial_marking) || has_negative(final_marking)) {
    return "marking contains a negative token count";
  }
  return {};
}

template <class Options>
std::string validate_token_evaluation_markings(const PetriNet& net, const Options& options) {
  if (const auto error = validate_evaluation_markings(net,
          options.exact_initial_marking ? Marking{} : options.initial_marking,
          options.exact_final_marking ? Marking{} : options.final_marking); !error.empty()) return error;
  if ((options.exact_initial_marking && options.exact_initial_marking->size() != net.place_count()) ||
      (options.exact_final_marking && options.exact_final_marking->size() != net.place_count()))
    return "marking dimension does not match net";
  return {};
}

template <class Source, class Target>
void copy_token_markings(const Source& source, Target& target) {
  target.initial_marking = source.initial_marking;
  target.final_marking = source.final_marking;
  target.exact_initial_marking = source.exact_initial_marking;
  target.exact_final_marking = source.exact_final_marking;
  target.initial_marking_keys = source.initial_marking_keys;
  target.final_marking_keys = source.final_marking_keys;
  target.place_names = source.place_names;
  target.transition_names = source.transition_names;
  target.place_outgoing_transition_order = source.place_outgoing_transition_order;
}

template <class Options>
bool inherit_document_token_markings(const PnmlDocument& document, Options& options) {
  const auto inherit = [&](const TokenMarking& stored, const std::vector<bool>& keys,
                           auto& legacy, auto& exact, auto& membership) {
    if (exact || !legacy.empty()) return true;
    if (stored.size() != document.net.place_count()) return false;
    exact = stored;
    if (membership.empty()) membership = keys;
    return true;
  };
  if (!inherit(document.initial_marking, document.initial_marking_keys, options.initial_marking,
               options.exact_initial_marking, options.initial_marking_keys) ||
      !inherit(document.final_marking, document.final_marking_keys, options.final_marking,
               options.exact_final_marking, options.final_marking_keys)) return false;
  if (options.place_names.empty()) options.place_names = document.place_ids;
  if (options.transition_names.empty()) options.transition_names = document.transition_ids;
  if (!options.place_outgoing_transition_order)
    options.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  return true;
}

Marking evaluation_initial_marking(const PetriNet& net,
                                   const Marking& requested) {
  if (!requested.empty()) return requested;
  Marking marking(net.place_count(), 0);
  if (net.source() < marking.size()) marking[net.source()] = 1;
  return marking;
}

std::vector<std::size_t> unsigned_marking(const Marking& marking) {
  std::vector<std::size_t> converted;
  converted.reserve(marking.size());
  for (const int tokens : marking) {
    converted.push_back(static_cast<std::size_t>(tokens));
  }
  return converted;
}

bool signed_marking(const std::vector<std::size_t>& marking,
                    Marking& converted) {
  converted.clear();
  converted.reserve(marking.size());
  for (const std::size_t tokens : marking) {
    if (tokens > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      converted.clear();
      return false;
    }
    converted.push_back(static_cast<int>(tokens));
  }
  return true;
}

bool valid_token_replay_variant(TokenReplayVariant variant) {
  switch (variant) {
    case TokenReplayVariant::kForward:
    case TokenReplayVariant::kBackwards:
      return true;
  }
  return false;
}

bool valid_alignment_variant_for_evaluation(AlignmentVariant variant) {
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

bool valid_alignment_cost_profile_for_evaluation(
    AlignmentCostProfile profile) {
  switch (profile) {
    case AlignmentCostProfile::kLegacyUnit:
    case AlignmentCostProfile::kStandard:
      return true;
  }
  return false;
}

std::string validate_alignment_evaluation_options(
    const PetriNet& net, const AlignmentOptions& options) {
  if (!valid_alignment_variant_for_evaluation(options.variant)) {
    return "unsupported alignment variant";
  }
  if (!valid_alignment_cost_profile_for_evaluation(options.cost_profile)) {
    return "unsupported alignment cost profile";
  }
  if (const auto error = validate_evaluation_markings(
          net, options.initial_marking, options.final_marking);
      !error.empty()) {
    return error;
  }
  const auto valid_cost = [](double value) {
    return std::isfinite(value) && value >= 0.0;
  };
  const auto valid_table = [&](const auto& table) {
    for (const auto& [_, value] : table) {
      if (!valid_cost(value)) return false;
    }
    return true;
  };
  const auto valid_vector = [&](const auto& values) {
    for (const double value : values) {
      if (!valid_cost(value)) return false;
    }
    return true;
  };
  const auto valid_optional = [&](const std::optional<double>& value) {
    return !value || valid_cost(*value);
  };
  if (!valid_optional(options.default_trace_cost) ||
      !valid_optional(options.default_model_cost) ||
      !valid_optional(options.default_sync_cost) ||
      !valid_optional(options.default_silent_cost) ||
      !valid_table(options.trace_costs) ||
      !valid_table(options.model_costs) ||
      !valid_table(options.sync_costs) ||
      !valid_vector(options.trace_costs_by_position) ||
      !valid_vector(options.model_costs_by_transition) ||
      !valid_vector(options.sync_costs_by_transition)) {
    return "invalid alignment cost";
  }
  if (options.variant == AlignmentVariant::kDiscountedAStar &&
      (!std::isfinite(options.exponent) || options.exponent <= 0.0)) {
    return "invalid discounted alignment exponent";
  }
  return {};
}

std::string validate_decomposed_evaluation_options(
    const PetriNet& net, const DecomposedAlignmentOptions& options) {
  if (options.variant != DecomposedAlignmentVariant::kRecomposMaximal) {
    return "unsupported decomposed alignment variant";
  }
  if (options.max_states == 0 || options.max_total_states == 0) {
    return "invalid decomposed alignment state limit";
  }
  const auto valid_cost = [](double value) {
    return std::isfinite(value) && value >= 0.0;
  };
  if (!valid_cost(options.threshold_border_agreement) ||
      !valid_cost(options.max_time_trace) ||
      !valid_cost(options.max_time_total) ||
      !valid_cost(options.log_move_cost) ||
      !valid_cost(options.model_move_cost) ||
      !valid_cost(options.tau_model_cost) ||
      !valid_cost(options.sync_cost)) {
    return "invalid decomposed alignment cost";
  }
  const auto valid_table = [&](const auto& table) {
    for (const auto& [_, value] : table) {
      if (!valid_cost(value)) return false;
    }
    return true;
  };
  if (!valid_table(options.activity_log_costs) ||
      !valid_table(options.activity_model_costs) ||
      !valid_table(options.activity_sync_costs)) {
    return "invalid decomposed alignment cost";
  }
  if ((!options.initial_marking.empty() &&
       options.initial_marking.size() != net.place_count()) ||
      (!options.final_marking.empty() &&
       options.final_marking.size() != net.place_count())) {
    return "marking dimension does not match net";
  }
  return {};
}

bool has_reachable_final_marking(const PetriNet& net,
                                 const Marking& initial_marking = {},
                                 const Marking& final_marking = {}) {
  EventLog probe;
  probe.traces.push_back(Trace{});
  AlignmentOptions options{
      AlignmentVariant::kStateEquationAStar,
      AlignmentCostProfile::kStandard};
  options.initial_marking = initial_marking;
  options.final_marking = final_marking;
  return check_alignment(probe, net, options).computed;
}

}  // namespace

ReplayFitnessResult evaluate_replay_fitness(
    const EventLog& log, const PetriNet& net, ReplayFitnessVariant variant,
    const ReplayFitnessOptions& options) {
  enforce_evaluation_event_log_cap(log, options.max_rows_to_materialize);
  ReplayFitnessResult out;
  out.trace_count = log.trace_count();
  switch (variant) {
    case ReplayFitnessVariant::AUTO:
    case ReplayFitnessVariant::TOKEN_BASED:
    case ReplayFitnessVariant::ALIGNMENT_BASED:
      break;
    default:
      out.variant = variant;
      out.computed = false;
      out.error = "unsupported replay-fitness variant";
      return out;
  }
  if (variant != ReplayFitnessVariant::TOKEN_BASED &&
      (options.exact_initial_marking || options.exact_final_marking)) {
    out.variant = variant;
    out.computed = false;
    out.error = "exact markings require token-based replay fitness";
    return out;
  }
  if (const auto error = validate_evaluation_markings(
          net, options.exact_initial_marking ? std::vector<int>{} : options.initial_marking,
          options.exact_final_marking ? std::vector<int>{} : options.final_marking);
      !error.empty()) {
    out.variant = variant;
    out.computed = false;
    out.error = error;
    return out;
  }
  if ((options.exact_initial_marking &&
       options.exact_initial_marking->size() != net.place_count()) ||
      (options.exact_final_marking &&
       options.exact_final_marking->size() != net.place_count())) {
    out.variant = variant;
    out.computed = false;
    out.error = "marking dimension does not match net";
    return out;
  }
  AlignmentOptions alignment_options = options.alignment_options;
  alignment_options.activity_attribute = options.activity_attribute.empty()
                                             ? "concept:name"
                                             : options.activity_attribute;
  std::optional<DecomposedAlignmentOptions> decomposed_options =
      options.decomposed_alignment_options;
  if (variant == ReplayFitnessVariant::AUTO && decomposed_options) {
    const auto& decomposed = *decomposed_options;
    if (options.initial_marking.empty() &&
        !decomposed.initial_marking.empty() &&
        !signed_marking(decomposed.initial_marking,
                        alignment_options.initial_marking)) {
      out.variant = variant;
      out.computed = false;
      out.error = "marking token count exceeds alignment range";
      return out;
    }
    if (options.final_marking.empty() &&
        !decomposed.final_marking.empty() &&
        !signed_marking(decomposed.final_marking,
                        alignment_options.final_marking)) {
      out.variant = variant;
      out.computed = false;
      out.error = "marking token count exceeds alignment range";
      return out;
    }
  }
  if (!options.initial_marking.empty()) {
    alignment_options.initial_marking = options.initial_marking;
  }
  if (!options.final_marking.empty()) {
    alignment_options.final_marking = options.final_marking;
  }
  if (variant != ReplayFitnessVariant::TOKEN_BASED && decomposed_options) {
    if (!options.initial_marking.empty()) {
      decomposed_options->initial_marking =
          unsigned_marking(options.initial_marking);
    }
    if (!options.final_marking.empty()) {
      decomposed_options->final_marking =
          unsigned_marking(options.final_marking);
    }
  }
  if (variant == ReplayFitnessVariant::AUTO) {
    if (const auto error = validate_evaluation_markings(
            net, alignment_options.initial_marking,
            alignment_options.final_marking);
        !error.empty()) {
      out.variant = variant;
      out.computed = false;
      out.error = error;
      return out;
    }
    variant = has_reachable_final_marking(
                  net, alignment_options.initial_marking,
                  alignment_options.final_marking)
                  ? ReplayFitnessVariant::ALIGNMENT_BASED
                  : ReplayFitnessVariant::TOKEN_BASED;
  }
  out.variant = variant;
  if (variant == ReplayFitnessVariant::TOKEN_BASED &&
      !valid_token_replay_variant(options.token_replay_variant)) {
    out.computed = false;
    out.error = "unsupported token replay variant";
    return out;
  }
  if (variant == ReplayFitnessVariant::ALIGNMENT_BASED &&
      !decomposed_options) {
    if (const auto error = validate_alignment_evaluation_options(
            net, alignment_options);
        !error.empty()) {
      out.computed = false;
      out.error = error;
      return out;
    }
  }
  if (variant == ReplayFitnessVariant::ALIGNMENT_BASED &&
      decomposed_options) {
    if (const auto error = validate_decomposed_evaluation_options(
            net, *decomposed_options);
        !error.empty()) {
      out.computed = false;
      out.error = error;
      return out;
    }
  }
  if (log.trace_count() == 0) {
    // Evaluators have no observations from which to compute a score;
    // unlike the generic conformance API, their empty-log metrics are zero.
    // Retain the selected detail engine so serialization does not substitute
    // an unrelated default payload for an otherwise valid empty evaluation.
    if (variant == ReplayFitnessVariant::TOKEN_BASED) {
      out.log_fitness_is_integer_zero = true;
      out.token_result_is_fitness_only = true;
      out.token_result.fitness = 0.0;
      out.token_result.activity_attribute = alignment_options.activity_attribute;
    } else if (decomposed_options) {
      out.decomposed_alignment_result = DecomposedAlignmentResult{};
    } else {
      out.alignment_result.variant = alignment_options.variant;
      out.alignment_result.activity_attribute =
          alignment_options.activity_attribute;
    }
    return out;
  }

  if (variant == ReplayFitnessVariant::TOKEN_BASED) {
    TokenReplayOptions replay_options;
    replay_options.variant = options.token_replay_variant;
    replay_options.stop_immediately_unfit = false;
    replay_options.consider_remaining_in_fitness = true;
    replay_options.try_to_reach_final_marking_through_hidden = true;
    replay_options.consider_activities_not_in_model_in_fitness =
        options.consider_activities_not_in_model_in_fitness;
    replay_options.activity_attribute = alignment_options.activity_attribute;
    replay_options.initial_marking = options.initial_marking;
    replay_options.final_marking = options.final_marking;
    replay_options.exact_initial_marking = options.exact_initial_marking;
    replay_options.exact_final_marking = options.exact_final_marking;
    replay_options.initial_marking_keys = options.initial_marking_keys;
    replay_options.final_marking_keys = options.final_marking_keys;
    replay_options.place_names = options.place_names;
    replay_options.transition_names = options.transition_names;
    replay_options.cleaning_token_flood = options.cleaning_token_flood;
    replay_options.place_outgoing_transition_order = options.place_outgoing_transition_order;
    std::vector<TokenReplayTraceDiagnostics> rows;
    try {
      rows = options.token_replay_variant == TokenReplayVariant::kForward
                 ? replay_token_result(log, net, replay_options).traces
                 : replay_token_traces(log, net, replay_options);
    } catch (const std::overflow_error& error) {
      if (std::string_view(error.what()) ==
          "token integer does not fit floating-point range")
        throw std::overflow_error("int too large to convert to float");
      throw;
    } catch (const std::domain_error& error) {
      if (std::string_view(error.what()) ==
          "backwards token-replay fitness is undefined when consumed or produced tokens are zero")
        throw std::domain_error("float division by zero");
      throw;
    }
    out = evaluate_replay_fitness(rows);
    out.token_result.activity_attribute = alignment_options.activity_attribute;
    out.token_result.transition_frequencies.assign(net.transitions().size(), 0);
    for (const auto& row : rows) {
      for (const auto transition : row.activated_transitions)
        ++out.token_result.transition_frequencies.at(transition);
    }
    // Retain per-trace variant details rather than collapsing value-distinct
    // typed activities which share the same display spelling.
    for (std::size_t i = 0; i < rows.size(); ++i) {
      auto& detail = out.token_result.variants[i];
      for (const auto& event : log.traces[i].events) {
        const bool primary = options.activity_attribute.empty() ||
                             options.activity_attribute == "concept:name" ||
                             options.activity_attribute == "activity";
        const bool first_class = primary ||
            event.activity_source_key == alignment_options.activity_attribute ||
            (event.activity_attribute &&
             event.activity_attribute->key == alignment_options.activity_attribute);
        const EventAttribute* attribute = first_class && event.activity_attribute
            ? &*event.activity_attribute : nullptr;
        if (!first_class) {
          const auto entry = std::find_if(event.attributes.begin(), event.attributes.end(),
              [&](const auto& value) { return value.key == alignment_options.activity_attribute; });
          if (entry != event.attributes.end()) attribute = &*entry;
        }
        if ((attribute && attribute->is_null) ||
            (first_class && event.activity_state == FirstClassAttributeState::kNull)) {
          detail.null_activity_indices.push_back(detail.activities.size());
          detail.activities.emplace_back();
        } else {
          detail.activities.push_back(selected_evaluation_activity(
              event, alignment_options.activity_attribute));
        }
      }
    }
    std::stable_sort(out.token_result.variants.begin(), out.token_result.variants.end(),
        [](const auto& a, const auto& b) {
          if (std::isnan(a.fitness)) return false;
          if (std::isnan(b.fitness)) return true;
          return a.fitness < b.fitness;
        });
    return out;
  } else if (decomposed_options) {
    const auto projected = project_evaluation_activity(
        log, alignment_options.activity_attribute);
    auto decomposed = align_decomposed(projected, net, *decomposed_options);
    out.computed = decomposed.complete;
    out.exhausted = decomposed.exhausted || decomposed.timed_out;
    out.fitness = out.computed ? decomposed.fitness : 0.0;
    out.log_fitness = out.fitness;
    out.total_cost = decomposed.total_cost;
    double average = 0.0;
    for (const auto& trace : decomposed.traces) {
      average += trace.fitness * double(trace.count);
      if (trace.complete && std::abs(trace.fitness - 1.0) <= 1e-12) {
        out.fitting_traces += trace.count;
      }
    }
    out.average_trace_fitness = average / double(out.trace_count);
    if (!out.computed) {
      out.error = decomposed.timed_out
                      ? "decomposed alignment timed out"
                      : decomposed.exhausted
                            ? "decomposed alignment search exhausted"
                            : "decomposed alignment failed";
    }
    out.decomposed_alignment_result = std::move(decomposed);
  } else {
    out.alignment_result = check_alignment(log, net, alignment_options);
    out.computed = out.alignment_result.computed;
    out.exhausted = out.alignment_result.exhausted;
    out.fitness = out.computed ? out.alignment_result.fitness : 0.0;
    out.log_fitness = out.fitness;
    out.fitting_traces = out.alignment_result.fitting_traces;
    out.total_cost = out.alignment_result.total_cost;
    double average = 0.0;
    if (alignment_options.cost_profile == AlignmentCostProfile::kStandard) {
      // The reference evaluator scores each trace on whole standard moves:
      // 1 - floor(cost / 10000) / floor(best-worst / 10000), so silent-move
      // epsilons never lower a trace below unit fitness. A trace whose
      // best-worst cost has no whole standard move scores zero. Log fitness
      // keeps the raw summed costs.
      out.fitting_traces = 0;
      for (const auto& alignment_variant : out.alignment_result.variants) {
        const double worst = std::floor(alignment_variant.worst_cost / 10000.0);
        const double fitness =
            worst > 0.0 ? 1.0 - std::floor(alignment_variant.cost / 10000.0) / worst : 0.0;
        average += fitness * double(alignment_variant.count);
        if (fitness == 1.0) out.fitting_traces += alignment_variant.count;
      }
    } else {
      for (const auto& alignment_variant : out.alignment_result.variants) {
        average += alignment_variant.fitness * double(alignment_variant.count);
      }
    }
    out.average_trace_fitness = average / double(out.trace_count);
    if (!out.computed) {
      out.error = out.exhausted ? "alignment search exhausted"
                                : "final marking is unreachable";
    }
  }
  out.percentage_fit_traces =
      100.0 * double(out.fitting_traces) / double(out.trace_count);
  return out;
}
ReplayFitnessResult evaluate_replay_fitness(const EventLog& l,const ProcessTree& t,ReplayFitnessVariant v) { return evaluate_replay_fitness(l,to_petri_net(t),v); }
ReplayFitnessResult evaluate_replay_fitness(
    const EventLog& log, const PnmlDocument& document, ReplayFitnessVariant variant,
    const ReplayFitnessOptions& options) {
  auto supplied = options;
  const auto set_marking = [&](const auto& marking, const auto& keys,
                               auto& legacy, auto& exact, auto& membership) {
    if (!exact && legacy.empty()) {
      if (variant == ReplayFitnessVariant::TOKEN_BASED) {
        exact = TokenMarking(marking);
        if (membership.empty()) membership = keys;
      } else {
        for (const auto value : marking) {
          if (value < 0 || value > std::numeric_limits<int>::max()) return false;
          legacy.push_back(static_cast<int>(value));
        }
      }
    }
    return true;
  };
  if (!set_marking(document.initial_marking, document.initial_marking_keys,
                   supplied.initial_marking, supplied.exact_initial_marking,
                   supplied.initial_marking_keys) ||
      !set_marking(document.final_marking, document.final_marking_keys,
                   supplied.final_marking, supplied.exact_final_marking,
                   supplied.final_marking_keys)) {
    ReplayFitnessResult out;
    out.variant = variant;
    out.computed = false;
    out.error = "PNML marking exceeds alignment token range";
    return out;
  }
  if (supplied.place_names.empty()) supplied.place_names = document.place_ids;
  if (supplied.transition_names.empty()) supplied.transition_names = document.transition_ids;
  if (!supplied.place_outgoing_transition_order)
    supplied.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  return evaluate_replay_fitness(log, document.net, variant, supplied);
}
ReplayFitnessResult evaluate_replay_fitness(
    const EventLog& l, const ProcessTree& t, ReplayFitnessVariant v,
    const ReplayFitnessOptions& o) {
  return evaluate_replay_fitness(l, to_petri_net(t), v, o);
}

ReplayFitnessResult evaluate_replay_fitness(
    const EventDataView& data, const PnmlDocument& document,
    ReplayFitnessVariant variant, const ReplayFitnessOptions& options) {
  return evaluate_replay_fitness(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      document, variant, options);
}

ReplayFitnessResult evaluate_replay_fitness(
    const EventDataView& data, const PetriNet& net, ReplayFitnessVariant variant,
    const ReplayFitnessOptions& options) {
  return evaluate_replay_fitness(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      net, variant, options);
}

ReplayFitnessResult evaluate_replay_fitness(
    const EventDataView& data, const ProcessTree& tree,
    ReplayFitnessVariant variant, const ReplayFitnessOptions& options) {
  return evaluate_replay_fitness(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      tree, variant, options);
}

ReplayFitnessResult evaluate_replay_fitness(
    const std::vector<TokenReplayTraceDiagnostics>& results,
    ReplayFitnessVariant variant) {
  ReplayFitnessResult output;
  output.variant = variant;
  output.log_fitness_is_integer_zero = true;
  output.token_result_is_fitness_only = true;
  output.token_result.fitness = 0.0;
  output.token_result.precision = 0.0;
  output.token_result.generalization = 0.0;
  output.token_result.simplicity = 0.0;
  if (variant != ReplayFitnessVariant::TOKEN_BASED) {
    output.computed = false;
    output.error = "precomputed token results require token-based replay";
    return output;
  }
  output.trace_count = results.size();
  output.token_result.trace_count = output.trace_count;
  double fitness_sum = 0.0;
  for (const auto& result : results) {
    output.fitting_traces += result.trace_is_fit ? 1 : 0;
    fitness_sum += result.trace_fitness;
    output.token_result.produced += result.produced;
    output.token_result.consumed += result.consumed;
    output.token_result.missing += result.missing;
    output.token_result.remaining += result.remaining;
    output.token_result.variants.push_back(
        VariantConformance{{}, 1, result.trace_fitness});
  }
  output.token_result.fitting_traces = output.fitting_traces;
  output.token_result.trace_diagnostics = results;
  if (output.trace_count > 0 && output.token_result.consumed > 0 &&
      output.token_result.produced > 0) {
    output.average_trace_fitness = fitness_sum / double(output.trace_count);
    output.percentage_fit_traces =
        100.0 * double(output.fitting_traces) / double(output.trace_count);
    output.fitness =
        0.5 * (1.0 - token_integer_ratio(output.token_result.missing,
                                        output.token_result.consumed)) +
        0.5 * (1.0 - token_integer_ratio(output.token_result.remaining,
                                        output.token_result.produced));
    output.log_fitness_is_integer_zero = false;
  }
  output.log_fitness = output.fitness;
  output.token_result.fitness = output.fitness;
  return output;
}

ReplayFitnessResult evaluate_replay_fitness(const TokenReplayResult& results,
                                           ReplayFitnessVariant variant) {
  return evaluate_replay_fitness(results.traces, variant);
}

ReplayFitnessResult evaluate_replay_fitness(
    const std::vector<PrecomputedTokenReplay>& results,
    ReplayFitnessVariant variant) {
  std::vector<TokenReplayTraceDiagnostics> rows;
  rows.reserve(results.size());
  std::vector<TokenInteger> transition_frequencies;
  bool exhausted = false;
  for (const auto& result : results) {
    TokenReplayTraceDiagnostics row;
    row.trace_index = rows.size();
    row.trace_is_fit = result.trace_is_fit;
    row.trace_fitness = result.trace_fitness;
    row.missing = result.missing_tokens.value_or(result.missing);
    row.consumed = result.consumed_tokens.value_or(result.consumed);
    row.remaining = result.remaining_tokens.value_or(result.remaining);
    row.produced = result.produced_tokens.value_or(result.produced);
    rows.push_back(std::move(row));
    exhausted = exhausted || result.exhausted;
    if (transition_frequencies.size() < result.transition_activations.size())
      transition_frequencies.resize(result.transition_activations.size());
    for (std::size_t i = 0; i < result.transition_activations.size(); ++i)
      transition_frequencies[i] += result.transition_activations[i];
  }
  auto output = evaluate_replay_fitness(rows, variant);
  output.exhausted = exhausted;
  // Activation details are optional native metadata. Their bounded legacy
  // projection must never prevent the exact public fitness calculation.
  for (const auto& frequency : transition_frequencies) {
    if (frequency > std::numeric_limits<std::uint64_t>::max()) {
      output.token_result.transition_frequencies.clear();
      break;
    }
    output.token_result.transition_frequencies.push_back(
        static_cast<std::uint64_t>(frequency));
  }
  return output;
}

ReplayFitnessResult evaluate_replay_fitness(
    const std::vector<PrecomputedAlignmentReplay>& results,
    ReplayFitnessVariant variant) {
  ReplayFitnessResult output;
  output.variant = variant;
  if (variant != ReplayFitnessVariant::ALIGNMENT_BASED) {
    output.computed = false;
    output.error = "precomputed alignment results require alignment-based replay";
    return output;
  }

  double fitness_sum = 0.0;
  double total_best_worst = 0.0;
  double total_cost = 0.0;
  std::uint64_t trace_count = 0;
  std::uint64_t fitting_traces = 0;
  std::vector<VariantAlignment> variants;
  for (const auto& result : results) {
    if (!result.computed) {
      // Null/incomplete alignment entries are intentionally excluded from
      // the aggregate, matching the evaluator's treatment of absent traces.
      continue;
    }
    output.exhausted = output.exhausted || result.exhausted;
    if (!std::isfinite(result.fitness) || result.fitness < 0.0 ||
        result.fitness > 1.0 || !std::isfinite(result.cost) ||
        result.cost < 0.0) {
      output.computed = false;
      output.error = "invalid precomputed alignment result";
      return output;
    }
    // Consumers commonly serialize the denominator as `bwc`; retain the
    // descriptive native spelling as the primary field while accepting the
    // wire-compatible alias when it is the only value supplied.
    const double best_worst_cost =
        result.best_worst_cost != 0.0 ? result.best_worst_cost : result.bwc;
    if (!std::isfinite(best_worst_cost) || best_worst_cost < 0.0) {
      output.computed = false;
      output.error = "invalid precomputed alignment result";
      return output;
    }
    if (trace_count == std::numeric_limits<std::uint64_t>::max() ||
        (result.fitness == 1.0 &&
         fitting_traces == std::numeric_limits<std::uint64_t>::max())) {
      output.computed = false;
      output.error = "precomputed alignment counter overflow";
      return output;
    }
    const double next_total_cost = total_cost + result.cost;
    const double next_fitness_sum = fitness_sum + result.fitness;
    const double next_total_best_worst = total_best_worst + best_worst_cost;
    if (!std::isfinite(next_total_cost) ||
        !std::isfinite(next_fitness_sum) ||
        !std::isfinite(next_total_best_worst)) {
      output.computed = false;
      output.error = "precomputed alignment metric overflow";
      return output;
    }
    ++trace_count;
    if (result.fitness == 1.0) ++fitting_traces;
    total_cost = next_total_cost;
    fitness_sum = next_fitness_sum;
    total_best_worst = next_total_best_worst;
    VariantAlignment variant_result;
    variant_result.count = 1;
    variant_result.fitness = result.fitness;
    variant_result.cost = result.cost;
    variant_result.worst_cost = best_worst_cost;
    variants.push_back(std::move(variant_result));
  }

  output.trace_count = trace_count;
  output.fitting_traces = fitting_traces;
  output.total_cost = total_cost;
  output.average_trace_fitness = output.trace_count == 0
                                     ? 0.0
                                     : fitness_sum / output.trace_count;
  output.percentage_fit_traces = output.trace_count == 0
                                     ? 0.0
                                     : 100.0 * output.fitting_traces /
                                           output.trace_count;
  // The cost-based evaluator reports a unit score for a nonempty collection
  // whose best-worst denominator is zero.
  if (output.trace_count > 0) {
    output.fitness = total_best_worst > 0.0
                         ? 1.0 - output.total_cost / total_best_worst
                         : 1.0;
  }
  output.log_fitness = output.fitness;
  output.alignment_result.computed = true;
  output.alignment_result.trace_count = output.trace_count;
  output.alignment_result.fitting_traces = output.fitting_traces;
  output.alignment_result.total_cost = output.total_cost;
  output.alignment_result.fitness = output.fitness;
  output.alignment_result.variant = AlignmentVariant::kDijkstraLessMemory;
  output.alignment_result.activity_attribute = "concept:name";
  output.alignment_result.variants = std::move(variants);
  return output;
}

namespace {

struct PrefixInfo {
  std::uint64_t frequency = 0;
  std::set<std::string> observed;
  Trace sample;
};

std::map<std::vector<std::string>, PrefixInfo> collect_prefixes(
    const EventLog& log, std::string_view activity_attribute) {
  std::map<std::vector<std::string>, PrefixInfo> prefixes;
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    for (const auto& event : trace.events) {
      activities.push_back(
          selected_evaluation_activity(event, activity_attribute));
    }
    auto& root = prefixes[{}];
    root.frequency += 1;
    if (!activities.empty()) root.observed.insert(activities.front());
    std::vector<std::string> prefix;
    for (std::size_t i = 0; i + 1 < activities.size(); ++i) {
      prefix.push_back(activities[i]);
      auto& node = prefixes[prefix];
      node.frequency += 1;
      node.observed.insert(activities[i + 1]);
    }
  }
  return prefixes;
}

struct TokenPrefixScore {
  std::uint64_t frequency = 0;
  std::set<std::string> observed;
};

struct TokenPrefixes {
  TokenPrefixScore initial;
  std::vector<TokenPrefixScore> proper;  // aligned with trie.scored
  detail::TokenPrefixTrie trie;
};

// Groups proper prefixes the way the public prefix utility does: string labels
// are joined with commas and the key is split again for replay, so a label
// containing a comma replays as several events and prefixes whose joined keys
// coincide are one prefix. Steps without a string label (null or typed
// activities) keep their own identity and replay as labels absent from the
// model. The prefixes form a trie over the split tokens, so storage is linear
// in the distinct prefix steps rather than in their total length.
TokenPrefixes collect_token_prefixes(const std::vector<detail::TokenPrecisionSequence>& sequences,
                                    const PetriNet& net,
                                    const std::string& activity_attribute) {
  TokenPrefixes result;
  auto& trie = result.trie;
  std::set<std::string> model_labels(net.activities().begin(), net.activities().end());
  std::size_t absent_count = 0;
  // Token text ("s" + label or "x" + identity) -> index into trie.labels.
  std::unordered_map<std::string, std::size_t> tokens;
  const auto token_label = [&](const std::string& token) {
    const auto [it, inserted] = tokens.try_emplace(token, trie.labels.size());
    if (inserted) {
      std::string label;
      if (token.front() == 's') {
        label = token.substr(1);
      } else {
        label = "\x1f" + std::to_string(++absent_count);
        while (model_labels.contains(label)) label += '\x1f';
      }
      Event event;
      event.activity = label;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key = activity_attribute;
      if (activity_attribute != "concept:name")
        event.attributes.push_back({activity_attribute, label, AttributeKind::kString});
      trie.labels.push_back(std::move(event));
    }
    return it->second;
  };
  struct EdgeHash {
    std::size_t operator()(const std::pair<std::size_t, std::size_t>& edge) const noexcept {
      return std::hash<std::size_t>{}(edge.first * 0x9e3779b97f4a7c15ULL ^ edge.second);
    }
  };
  std::unordered_map<std::pair<std::size_t, std::size_t>, std::size_t, EdgeHash> children;
  std::vector<std::size_t> scored_index{std::numeric_limits<std::size_t>::max()};
  const auto child = [&](std::size_t node, std::size_t label) {
    const auto [it, inserted] = children.try_emplace({node, label}, trie.parent.size());
    if (inserted) {
      trie.parent.push_back(node);
      trie.label.push_back(label);
      scored_index.push_back(std::numeric_limits<std::size_t>::max());
    }
    return it->second;
  };
  const auto advance = [&](const detail::TokenPrecisionStep& step, std::size_t node) {
    if (!step.text) return child(node, token_label("x" + step.identity));
    std::size_t begin = 0;
    while (true) {
      const auto end = step.text->find(',', begin);
      node = child(node, token_label(
          "s" + step.text->substr(begin, end == std::string::npos ? end : end - begin)));
      if (end == std::string::npos) return node;
      begin = end + 1;
    }
  };
  for (const auto& sequence : sequences) {
    const auto& steps = sequence.steps;
    result.initial.frequency += sequence.count;
    if (!steps.empty() && steps.front().text) result.initial.observed.insert(*steps.front().text);
    std::size_t node = 0;
    for (std::size_t i = 0; i + 1 < steps.size(); ++i) {
      node = advance(steps[i], node);
      if (scored_index[node] == std::numeric_limits<std::size_t>::max()) {
        scored_index[node] = result.proper.size();
        result.proper.emplace_back();
        trie.scored.push_back(node);
      }
      auto& prefix = result.proper[scored_index[node]];
      prefix.frequency += sequence.count;
      if (steps[i + 1].text) prefix.observed.insert(*steps[i + 1].text);
    }
  }
  return result;
}

// The materialised trace of a scored prefix (backwards replay needs whole
// prefixes): the label events along its trie path.
Trace token_prefix_trace(const detail::TokenPrefixTrie& trie, std::size_t node, std::size_t index) {
  Trace trace;
  trace.case_id = "prefix-" + std::to_string(index);
  for (; node != 0; node = trie.parent[node]) trace.events.push_back(trie.labels[trie.label[node]]);
  std::reverse(trace.events.begin(), trace.events.end());
  for (std::size_t position = 0; position < trace.events.size(); ++position) {
    trace.events[position].case_id = trace.case_id;
    trace.events[position].input_order = position;
  }
  return trace;
}

struct StopAlignmentResult {
  bool computed = true;
  bool exhausted = false;
  bool fit = false;
  double cost = 0.0;
  std::vector<Marking> markings;
};

struct ProductKey {
  Marking marking;
  std::size_t position = 0;
  bool operator==(const ProductKey& other) const {
    return position == other.position && marking == other.marking;
  }
};
struct ProductHash {
  std::size_t operator()(const ProductKey& key) const noexcept {
    std::size_t h = MarkingHash{}(key.marking);
    return (h ^ (key.position + 0x9e3779b9u)) * 1099511628211ull;
  }
};

StopAlignmentResult align_to_stop_prefix(const PetriNet& net,
                                          const std::vector<std::string>& prefix,
                                          const Marking& initial_marking,
                                          std::size_t cap) {
  StopAlignmentResult result;
  if (cap == 0) {
    result.computed = false;
    result.exhausted = true;
    return result;
  }
  struct State { Marking marking; std::size_t position; };
  struct QueueItem { double cost; std::size_t id; bool operator>(const QueueItem& o) const { return cost > o.cost; } };
  std::vector<State> states;
  std::vector<double> distances;
  std::unordered_map<ProductKey, std::size_t, ProductHash> ids;
  Marking start = initial_marking;
  ids.emplace(ProductKey{start, 0}, 0);
  states.push_back({start, 0});
  distances.push_back(0.0);
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
  queue.push({0.0, 0});
  constexpr double kSync = 0.0;
  constexpr double kTau = 1.0;
  double best = std::numeric_limits<double>::infinity();
  while (!queue.empty()) {
    const auto item = queue.top();
    queue.pop();
    if (item.cost != distances[item.id]) continue;
    if (item.cost > best) break;
    // Copy before relaxing successors: inserting a new product state may
    // reallocate `states` and invalidate references into the vector.
    const State state = states[item.id];
    if (state.position == prefix.size()) {
      result.fit = true;
      if (item.cost < best) {
        best = item.cost;
        result.markings.clear();
      }
      if (item.cost == best) result.markings.push_back(state.marking);
      continue;
    }
    auto relax = [&](const Marking& marking, std::size_t position, double cost) {
      const double next_cost = item.cost + cost;
      ProductKey key{marking, position};
      auto it = ids.find(key);
      if (it == ids.end()) {
        if (states.size() >= cap) {
          result.computed = false;
          result.exhausted = true;
          return;
        }
        const auto id = states.size();
        ids.emplace(std::move(key), id);
        states.push_back({marking, position});
        distances.push_back(next_cost);
        queue.push({next_cost, id});
      } else if (next_cost < distances[it->second]) {
        distances[it->second] = next_cost;
        queue.push({next_cost, it->second});
      }
    };
    for (const auto& transition : net.transitions()) {
      if (!enabled(transition, state.marking)) continue;
      const auto next = fire(transition, state.marking);
      if (transition.silent()) {
        relax(next, state.position, kTau);
      } else if (net.activities()[*transition.activity] ==
                 prefix[state.position]) {
        relax(next, state.position + 1, kSync);
      }
    }
    if (!result.computed) return result;
  }
  result.cost = result.fit ? best : 0.0;
  std::sort(result.markings.begin(), result.markings.end());
  result.markings.erase(std::unique(result.markings.begin(), result.markings.end()), result.markings.end());
  return result;
}

// Align-ETConformance observes prefix states in which every ordinary output
// arc produces a single token, while input arcs keep their weights (observed on
// the pinned public evaluator with weighted arcs). Repeated output endpoints
// therefore also collapse to one produced token.
PetriNet unit_output_net(const PetriNet& net) {
  PetriNet copy = net;
  for (std::size_t index = 0; index < net.transitions().size(); ++index) {
    const auto& transition = net.transitions()[index];
    std::vector<PetriArcWeight> outputs;
    std::set<std::size_t> seen;
    bool changed = false;
    for (std::size_t arc = 0; arc < transition.outputs.size(); ++arc) {
      const auto weight = transition.output_arc_weight(arc);
      if (weight.negative || weight.magnitude == 0) {
        outputs.push_back(weight);
        continue;
      }
      const bool first = seen.insert(transition.outputs[arc]).second;
      outputs.push_back({first ? 1 : 0, false});
      changed = changed || !first || weight.magnitude != 1;
    }
    if (changed) copy.set_ordinary_arc_weights(index, transition.input_weights, std::move(outputs));
  }
  return copy;
}

PrecisionResult evaluate_precision_impl(const EventLog& log, const PetriNet& net,
                                         PrecisionVariant variant,
                                         const PrecisionOptions& options) {
  enforce_evaluation_event_log_cap(log, options.max_rows_to_materialize);
  PrecisionResult out;
  out.variant = variant;
  switch (variant) {
    case PrecisionVariant::AUTO:
    case PrecisionVariant::ETCONFORMANCE_TOKEN:
    case PrecisionVariant::ALIGN_ETCONFORMANCE:
    case PrecisionVariant::AUTOMATON_AFTER_ALIGN:
      break;
    default:
      out.computed = false;
      out.error = "unsupported precision variant";
      return out;
  }
  if (const auto error = validate_token_evaluation_markings(net, options);
      !error.empty()) {
    out.computed = false;
    out.error = error;
    return out;
  }
  const std::string activity_attribute = options.activity_attribute.empty()
                                             ? "concept:name"
                                             : options.activity_attribute;
  if (variant != PrecisionVariant::ETCONFORMANCE_TOKEN &&
      (options.exact_initial_marking || options.exact_final_marking)) {
    out.computed = false;
    out.error = "exact markings require token-based precision";
    return out;
  }
  AlignmentOptions alignment_options{
      AlignmentVariant::kDijkstraLessMemory,
      AlignmentCostProfile::kStandard};
  alignment_options.activity_attribute = activity_attribute;
  alignment_options.initial_marking = options.initial_marking;
  alignment_options.final_marking = options.final_marking;
  if (options.max_states == 0) {
    out.computed = false;
    out.exhausted = true;
    out.error = "precision state cap must be positive";
    return out;
  }
  if (variant == PrecisionVariant::AUTO) {
    variant = has_reachable_final_marking(
                  net, options.initial_marking, options.final_marking)
                  ? PrecisionVariant::ALIGN_ETCONFORMANCE
                  : PrecisionVariant::ETCONFORMANCE_TOKEN;
  }
  out.variant = variant;
  if (variant == PrecisionVariant::ETCONFORMANCE_TOKEN) {
    if (!valid_token_replay_variant(options.token_replay_variant)) {
      out.computed = false;
      out.error = "unsupported token replay variant";
      return out;
    }
    TokenReplayOptions replay_options;
    replay_options.variant = options.token_replay_variant;
    replay_options.stop_immediately_unfit = true;
    replay_options.consider_remaining_in_fitness = false;
    replay_options.try_to_reach_final_marking_through_hidden = false;
    replay_options.consider_activities_not_in_model_in_fitness = false;
    replay_options.activity_attribute = activity_attribute;
    copy_token_markings(options, replay_options);
    out.precision = detail::token_etconformance_precision(log, net, replay_options);
    out.token_result.precision = out.precision;
    out.token_result.activity_attribute = activity_attribute;
    out.token_result_is_precision_only = true;
    return out;
  }

  const Marking initial_marking = evaluation_initial_marking(net, options.initial_marking);

  // Check model easy-soundness independently of the input log.  The legacy
  // alignment API intentionally treats an empty log as trivially computable,
  // which would otherwise mask an unreachable final marking here.
  EventLog probe;
  probe.traces.push_back(Trace{});
  const auto soundness = check_alignment(probe, net, alignment_options);
  if (!soundness.computed) {
    out.computed = false;
    out.exhausted = soundness.exhausted;
    out.error = out.exhausted ? "alignment search exhausted" : "model is not easy sound";
    return out;
  }

  if (variant == PrecisionVariant::ALIGN_ETCONFORMANCE) {
    const auto prefix_net = unit_output_net(net);
    const auto name_order = transitions_in_name_order(prefix_net, options.transition_names);
    const auto prefixes = collect_prefixes(log, activity_attribute);
    double enabled_total = 0.0;
    double escaping_total = 0.0;
    for (const auto& [prefix, info] : prefixes) {
      if (info.frequency == 0) continue;
      std::vector<Marking> markings;
      if (prefix.empty()) {
        markings.push_back(initial_marking);
      } else {
        auto stop = align_to_stop_prefix(
            prefix_net, prefix, initial_marking, options.max_states);
        if (!stop.computed) {
          out.computed = false;
          out.exhausted = stop.exhausted;
          out.error = "precision search exhausted";
          return out;
        }
        if (!stop.fit) continue;
        markings = std::move(stop.markings);
      }
      std::set<std::string> available;
      for (const auto& marking : markings) {
        std::set<std::string> local;
        if (!eventually_enabled_labels(prefix_net, marking, name_order, options.max_states,
                                       false, local)) {
          out.computed = false;
          out.exhausted = true;
          out.error = "silent closure state cap exceeded";
          return out;
        }
        available.insert(local.begin(), local.end());
      }
      enabled_total += double(info.frequency) * available.size();
      for (const auto& symbol : available)
        if (!info.observed.count(symbol)) escaping_total += double(info.frequency);
    }
    out.precision = enabled_total == 0.0
                       ? 1.0
                       : std::clamp(1.0 - escaping_total / enabled_total, 0.0, 1.0);
    return out;
  }

  out.alignment_result = check_alignment(log, net, alignment_options);
  if (!out.alignment_result.computed) {
    out.computed = false;
    out.exhausted = out.alignment_result.exhausted;
    out.error = out.exhausted ? "alignment search exhausted" : "alignment failed";
    return out;
  }
  out.alignment_detail_present = true;

  std::vector<detail::AutomatonPrecisionPath> paths;
  paths.reserve(out.alignment_result.variants.size());
  for (const auto& variant_alignment : out.alignment_result.variants)
    paths.push_back({variant_alignment.count, variant_alignment.projected_model_moves});
  const auto score = detail::automaton_after_align_precision(
      net, initial_marking, paths, options.transition_names, options.max_states,
      options.use_task_ids);
  if (!score.computed) {
    out.computed = false;
    out.exhausted = true;
    out.error = "silent closure state cap exceeded";
    return out;
  }
  out.precision = score.precision;
  return out;
}
}  // namespace

namespace detail {

double token_etconformance_precision(const EventLog& log, const PetriNet& net,
                                     TokenReplayOptions replay_options) {
  const std::string activity_attribute = replay_options.activity_attribute.empty()
      ? "concept:name" : replay_options.activity_attribute;
  std::vector<TokenPrecisionSequence> sequences;
  sequences.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    auto& sequence = sequences.emplace_back();
    for (const auto& event : trace.events)
      sequence.steps.push_back({selected_evaluation_activity(event, activity_attribute), {}});
  }
  return token_etconformance_precision(sequences, net, std::move(replay_options));
}

double token_etconformance_precision(const std::vector<TokenPrecisionSequence>& sequences,
                                     const PetriNet& net,
                                     TokenReplayOptions replay_options) {
  // The reference evaluator replays every distinct proper prefix as its own
  // trace, stopping at the first missing token and never routing to the final
  // marking, then scores the labels eventually enabled at the reached marking.
  replay_options.stop_immediately_unfit = true;
  replay_options.consider_remaining_in_fitness = false;
  replay_options.try_to_reach_final_marking_through_hidden = false;
  replay_options.consider_activities_not_in_model_in_fitness = false;
  const std::string activity_attribute = replay_options.activity_attribute.empty()
      ? "concept:name" : replay_options.activity_attribute;
  replay_options.activity_attribute = activity_attribute;
  const auto prefixes = collect_token_prefixes(sequences, net, activity_attribute);
  TokenInteger enabled_total = 0, escaping_total = 0;
  const auto score = [&](const TokenPrefixScore& prefix, const std::set<std::string>& labels) {
    enabled_total += TokenInteger(prefix.frequency) * labels.size();
    for (const auto& label : labels)
      if (!prefix.observed.contains(label)) escaping_total += prefix.frequency;
  };
  const auto score_enabled = [&](const TokenPrefixScore& prefix,
                                 const std::vector<std::size_t>& enabled) {
    std::set<std::string> labels;
    for (const auto transition : enabled) {
      const auto& node = net.transitions().at(transition);
      if (node.activity) labels.insert(net.activities().at(*node.activity));
    }
    score(prefix, labels);
  };
  if (replay_options.variant == TokenReplayVariant::kForward) {
    // Forward replay is incremental: each prefix continues from the state its
    // trie parent reached, so the walk costs about one replay step per trie
    // edge instead of one per event of every materialised prefix.
    detail::forward_token_prefix_walk(
        net, replay_options, prefixes.trie,
        [&](std::size_t index, const std::vector<std::size_t>* enabled) {
          if (enabled) score_enabled(prefixes.proper[index], *enabled);
        });
  } else {
    // Backwards replay needs whole prefixes; materialise them a bounded chunk
    // at a time so peak memory stays independent of the total prefix length.
    // The row limit applies to the whole prefix log, as one replay would.
    if (replay_options.max_rows_to_materialize) {
      std::vector<std::size_t> depth(prefixes.trie.parent.size(), 0);
      for (std::size_t node = 1; node < depth.size(); ++node)
        depth[node] = depth[prefixes.trie.parent[node]] + 1;
      std::size_t available = *replay_options.max_rows_to_materialize;
      for (const auto node : prefixes.trie.scored) {
        if (depth[node] > available)
          throw std::length_error("token replay input exceeds max_rows_to_materialize");
        available -= depth[node];
      }
      replay_options.max_rows_to_materialize.reset();
    }
    constexpr std::size_t kChunkEvents = 1 << 16;
    std::size_t next = 0;
    do {
      EventLog chunk;
      const std::size_t first = next;
      std::size_t events = 0;
      while (next < prefixes.trie.scored.size() && (events == 0 || events < kChunkEvents)) {
        chunk.traces.push_back(token_prefix_trace(prefixes.trie, prefixes.trie.scored[next], next));
        events += chunk.traces.back().events.size();
        ++next;
      }
      const auto rows = replay_token_traces(chunk, net, replay_options);
      for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].trace_is_fit) score_enabled(prefixes.proper[first + i], rows[i].enabled_transitions);
    } while (next < prefixes.trie.scored.size());
  }
  if (prefixes.initial.frequency != 0) {
    std::set<std::string> labels;
    for (const auto activity : detail::initial_enabled_token_activities(net, replay_options))
      labels.insert(net.activities().at(activity));
    score(prefixes.initial, labels);
  }
  return enabled_total == 0 ? 1.0 : 1.0 - token_integer_ratio(escaping_total, enabled_total);
}

bool eventually_enabled_labels(const PetriNet& net, const std::vector<int>& marking,
                               const std::vector<std::string>& transition_names,
                               std::size_t max_states, bool use_task_ids,
                               std::set<std::string>& labels) {
  return minera::eventually_enabled_labels(net, marking,
                                           transitions_in_name_order(net, transition_names),
                                           max_states, use_task_ids, labels);
}

AutomatonPrecisionScore automaton_after_align_precision(
    const PetriNet& net, const std::vector<int>& initial_marking,
    const std::vector<AutomatonPrecisionPath>& paths,
    const std::vector<std::string>& transition_names, std::size_t max_states,
    bool use_task_ids, std::map<std::vector<int>, std::set<std::string>>* available_cache) {
  // Automaton-after-align projects each optimal alignment to visible model
  // moves.  A shared prefix is weighted by variants that have another move;
  // terminal projected prefixes are never scored.
  struct Node {
    std::map<std::string, std::size_t> children;
    std::uint64_t frequency = 0;
    std::set<std::string> observed;
    std::vector<Marking> markings;
  };
  std::vector<Node> trie(1);
  for (const auto& path : paths) {
    auto& root = trie[0];
    root.frequency += path.count;
    root.markings.push_back(initial_marking);
    std::size_t node = 0;
    for (std::size_t i = 0; i < path.moves.size(); ++i) {
      const auto& move = path.moves[i];
      const std::string symbol = use_task_ids
                                     ? "t" + std::to_string(move.transition)
                                     : move.activity;
      trie[node].observed.insert(symbol);
      auto child = trie[node].children.find(symbol);
      if (child == trie[node].children.end()) {
        const auto id = trie.size();
        trie[node].children.emplace(symbol, id);
        trie.emplace_back();
        child = trie[node].children.find(symbol);
      }
      node = child->second;
      if (i + 1 < path.moves.size()) {
        trie[node].frequency += path.count;
        trie[node].markings.push_back(move.marking_after);
      }
    }
  }
  const auto order = transitions_in_name_order(net, transition_names);
  AutomatonPrecisionScore score;
  double enabled_total = 0.0;
  double escaping_total = 0.0;
  for (const auto& node : trie) {
    if (node.frequency == 0) continue;
    std::set<std::string> available;
    for (const auto& marking : node.markings) {
      if (available_cache) {
        if (const auto cached = available_cache->find(marking); cached != available_cache->end()) {
          available.insert(cached->second.begin(), cached->second.end());
          continue;
        }
      }
      std::set<std::string> local;
      if (!minera::eventually_enabled_labels(net, marking, order, max_states, use_task_ids, local)) {
        score.computed = false;
        return score;
      }
      available.insert(local.begin(), local.end());
      if (available_cache) available_cache->emplace(marking, std::move(local));
    }
    enabled_total += double(node.frequency) * available.size();
    for (const auto& symbol : available)
      if (!node.observed.count(symbol)) escaping_total += double(node.frequency);
  }
  score.precision = enabled_total == 0.0
                        ? 1.0
                        : std::clamp(1.0 - escaping_total / enabled_total, 0.0, 1.0);
  return score;
}

}  // namespace detail

PrecisionResult evaluate_precision(const EventLog& l, const PetriNet& n,
                                   PrecisionVariant v) {
  return evaluate_precision_impl(l, n, v, {});
}
PrecisionResult evaluate_precision(const EventLog& l, const ProcessTree& t,
                                   PrecisionVariant v) {
  return evaluate_precision(l, to_petri_net(t), v);
}
PrecisionResult evaluate_precision(const EventLog& l, const ProcessTree& t,
                                   PrecisionVariant v,
                                   const PrecisionOptions& o) {
  return evaluate_precision(l, to_petri_net(t), v, o);
}
PrecisionResult evaluate_precision(const EventLog& l, const PetriNet& n,
                                   PrecisionVariant v, const PrecisionOptions& o) {
  return evaluate_precision_impl(l, n, v, o);
}

PrecisionResult evaluate_precision(const EventLog& log, const PnmlDocument& document,
                                   PrecisionVariant variant, const PrecisionOptions& options) {
  auto supplied = options;
  if (variant != PrecisionVariant::ETCONFORMANCE_TOKEN) {
    // Alignment-based (and automatically dispatched) precision searches use
    // native int markings, as replay fitness does for its alignment variants.
    const auto set_marking = [&](const TokenMarking& marking, auto& legacy, const auto& exact) {
      if (exact || !legacy.empty()) return true;
      for (const auto value : marking) {
        if (value < 0 || value > std::numeric_limits<int>::max()) return false;
        legacy.push_back(static_cast<int>(value));
      }
      return true;
    };
    if (!set_marking(document.initial_marking, supplied.initial_marking,
                     supplied.exact_initial_marking) ||
        !set_marking(document.final_marking, supplied.final_marking,
                     supplied.exact_final_marking)) {
      PrecisionResult out;
      out.variant = variant;
      out.computed = false;
      out.error = "PNML marking exceeds alignment token range";
      return out;
    }
    if (supplied.place_names.empty()) supplied.place_names = document.place_ids;
    if (supplied.transition_names.empty()) supplied.transition_names = document.transition_ids;
  } else if (!inherit_document_token_markings(document, supplied)) {
    PrecisionResult out;
    out.variant = variant;
    out.computed = false;
    out.error = "PNML marking dimension does not match net";
    return out;
  }
  return evaluate_precision(log, document.net, variant, supplied);
}

PrecisionResult evaluate_precision(const EventDataView& data, const PnmlDocument& document,
                                   PrecisionVariant variant, const PrecisionOptions& options) {
  return evaluate_precision(evaluation_event_log(data, options.activity_attribute,
      options.case_id_attribute, options.max_rows_to_materialize), document, variant, options);
}

PrecisionResult evaluate_precision(const EventDataView& data,
                                   const PetriNet& net,
                                   PrecisionVariant variant,
                                   const PrecisionOptions& options) {
  return evaluate_precision_impl(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      net, variant, options);
}

PrecisionResult evaluate_precision(const EventDataView& data,
                                   const ProcessTree& tree,
                                   PrecisionVariant variant,
                                   const PrecisionOptions& options) {
  return evaluate_precision(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      tree, variant, options);
}

GeneralizationResult evaluate_generalization(
    const EventLog& log, const PetriNet& net, GeneralizationVariant variant) {
  return evaluate_generalization(log, net, variant, {});
}

GeneralizationResult evaluate_generalization(
    const EventLog& log, const PetriNet& net, GeneralizationVariant v,
    const GeneralizationOptions& options) {
  enforce_evaluation_event_log_cap(log, options.max_rows_to_materialize);
  GeneralizationResult out;
  out.variant = v;
  out.transition_count = net.transitions().size();
  out.transition_frequencies.assign(net.transitions().size(), 0);
  if (v != GeneralizationVariant::GENERALIZATION_TOKEN) {
    out.computed = false;
    out.error = "unsupported generalization variant";
    return out;
  }
  if (const auto error = validate_token_evaluation_markings(net, options);
      !error.empty()) {
    out.computed = false;
    out.error = error;
    return out;
  }
  if (log.trace_count() == 0) {
    out.generalization = net.transitions().empty() ? 1.0 : 0.0;
    return out;
  }
  TokenReplayOptions replay_options;
  replay_options.consider_activities_not_in_model_in_fitness = false;
  replay_options.activity_attribute = options.activity_attribute.empty()
                                          ? "concept:name"
                                          : options.activity_attribute;
  copy_token_markings(options, replay_options);
  const auto replay = replay_token_result(log, net, replay_options);
  for (const auto& row : replay.traces)
    for (const auto transition : row.activated_transitions)
      ++out.transition_frequencies.at(transition);
  if (out.transition_frequencies.size() < net.transitions().size())
    out.transition_frequencies.resize(net.transitions().size(), 0);
  for (const auto count : out.transition_frequencies)
    if (count) ++out.executed_transitions;
  double rarity = 0.0;
  for (const auto count : out.transition_frequencies)
    rarity += count ? 1.0 / std::sqrt(double(count)) : 1.0;
  out.generalization = out.transition_frequencies.empty()
                           ? 1.0
                           : 1.0 - rarity / double(out.transition_frequencies.size());
  return out;
}
GeneralizationResult evaluate_generalization(const EventLog& log, const PnmlDocument& document,
                                             GeneralizationVariant variant, const GeneralizationOptions& options) {
  auto supplied = options;
  if (!inherit_document_token_markings(document, supplied)) {
    GeneralizationResult out;
    out.variant = variant;
    out.computed = false;
    out.error = "PNML marking dimension does not match net";
    return out;
  }
  return evaluate_generalization(log, document.net, variant, supplied);
}

GeneralizationResult evaluate_generalization(const EventDataView& data, const PnmlDocument& document,
                                             GeneralizationVariant variant, const GeneralizationOptions& options) {
  return evaluate_generalization(evaluation_event_log(data, options.activity_attribute,
      options.case_id_attribute, options.max_rows_to_materialize), document, variant, options);
}

GeneralizationResult evaluate_generalization(
    const EventLog& log, const ProcessTree& tree,
    GeneralizationVariant variant) {
  return evaluate_generalization(log, to_petri_net(tree), variant, {});
}

GeneralizationResult evaluate_generalization(
    const EventLog& log, const ProcessTree& tree,
    GeneralizationVariant variant, const GeneralizationOptions& options) {
  return evaluate_generalization(log, to_petri_net(tree), variant, options);
}

GeneralizationResult evaluate_generalization(
    const EventDataView& data, const PetriNet& net,
    GeneralizationVariant variant, const GeneralizationOptions& options) {
  return evaluate_generalization(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      net, variant, options);
}

GeneralizationResult evaluate_generalization(
    const EventDataView& data, const ProcessTree& tree,
    GeneralizationVariant variant, const GeneralizationOptions& options) {
  return evaluate_generalization(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      tree, variant, options);
}

namespace {

void finish_cyclomatic(SimplicityResult& out,
                      const std::vector<std::vector<std::size_t>>& adjacency,
                      std::optional<double> scalar = std::nullopt) {
  out.vertices = adjacency.size();
  for (const auto& edges : adjacency) out.edges += edges.size();
  std::vector<std::vector<std::size_t>> reverse(out.vertices);
  for (std::size_t source = 0; source < out.vertices; ++source)
    for (const auto target : adjacency[source]) reverse[target].push_back(source);
  std::vector<char> seen(out.vertices, 0);
  std::vector<std::size_t> order;
  order.reserve(out.vertices);
  for (std::size_t root = 0; root < out.vertices; ++root) {
    if (seen[root]) continue;
    std::vector<std::pair<std::size_t, std::size_t>> stack{{root, 0}};
    seen[root] = 1;
    while (!stack.empty()) {
      auto& [node, edge] = stack.back();
      if (edge < adjacency[node].size()) {
        const auto target = adjacency[node][edge++];
        if (!seen[target]) { seen[target] = 1; stack.push_back({target, 0}); }
      } else { order.push_back(node); stack.pop_back(); }
    }
  }
  std::fill(seen.begin(), seen.end(), char{0});
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    if (seen[*it]) continue;
    ++out.strongly_connected_components;
    std::vector<std::size_t> stack{*it};
    seen[*it] = 1;
    while (!stack.empty()) {
      const auto node = stack.back(); stack.pop_back();
      for (const auto target : reverse[node]) {
        if (!seen[target]) { seen[target] = 1; stack.push_back(target); }
      }
    }
  }
  out.complexity = out.value = out.simplicity = scalar.value_or(double(out.edges));
}

using PresentedMarking = std::vector<std::pair<std::size_t, std::int64_t>>;

char32_t cyclomatic_codepoint(std::string_view value, std::size_t& position) {
  const auto first = static_cast<unsigned char>(value[position++]);
  char32_t codepoint = first;
  int continuation = 0;
  if (first >= 0xc2 && first <= 0xdf) { codepoint = first & 0x1f; continuation = 1; }
  else if (first >= 0xe0 && first <= 0xef) { codepoint = first & 0x0f; continuation = 2; }
  else if (first >= 0xf0 && first <= 0xf4) { codepoint = first & 0x07; continuation = 3; }
  else if (first >= 0x80) throw std::invalid_argument("invalid UTF-8 presented place name");
  for (int i = 0; i < continuation; ++i) {
    if (position == value.size()) throw std::invalid_argument("invalid UTF-8 presented place name");
    const auto next = static_cast<unsigned char>(value[position++]);
    if ((next & 0xc0) != 0x80) throw std::invalid_argument("invalid UTF-8 presented place name");
    codepoint = (codepoint << 6) | (next & 0x3f);
  }
  if ((continuation == 1 && codepoint < 0x80) ||
      (continuation == 2 && codepoint < 0x800) ||
      (continuation == 3 && codepoint < 0x10000) ||
      codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
    throw std::invalid_argument("invalid UTF-8 presented place name");
  return codepoint;
}

std::string cyclomatic_quoted_text(const std::string& text) {
  const char quote = text.find('\'') != std::string::npos && text.find('"') == std::string::npos ? '"' : '\'';
  std::string result(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t position = 0; position < text.size();) {
    const auto start = position;
    const auto codepoint = cyclomatic_codepoint(text, position);
    if (codepoint == '\\' || codepoint == static_cast<unsigned char>(quote)) {
      result += '\\'; result += static_cast<char>(codepoint);
    } else if (codepoint == '\n') result += "\\n";
    else if (codepoint == '\r') result += "\\r";
    else if (codepoint == '\t') result += "\\t";
    else if (!root_cause_unicode_printable(codepoint)) {
      const int digits = codepoint <= 0xff ? 2 : codepoint <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        result += hex[(codepoint >> shift) & 15];
    } else result.append(text, start, position - start);
  }
  result += quote;
  return result;
}

std::string cyclomatic_marking_name(const PetriNetPresentation& net,
                                   PresentedMarking marking) {
  std::stable_sort(marking.begin(), marking.end(), [&](const auto& left, const auto& right) {
    return net.places[left.first].name < net.places[right.first].name;
  });
  std::string name;
  for (const auto& [place, tokens] : marking) {
    const auto text = cyclomatic_quoted_text(net.places[place].name + ':' + std::to_string(tokens));
    for (std::size_t position = 0; position < text.size();) {
      const auto start = position;
      if (cyclomatic_unicode_word(cyclomatic_codepoint(text, position)))
        name.append(text, start, position - start);
    }
  }
  return name;
}

SimplicityResult evaluate_presented_cyclomatic(
    const PetriNetPresentation& net,
    const std::unordered_map<std::size_t, std::size_t>& places,
    const std::unordered_map<std::size_t, std::size_t>& transitions,
    const SimplicityOptions& options) {
  SimplicityResult out;
  out.variant = SimplicityVariant::EXTENDED_CYCLOMATIC;
  const auto reject = [&](const char* error) { out.computed = false; out.error = error; return out; };
  if (!options.max_states ||
      (!options.presentation_initial_marking && !options.initial_marking.empty() && options.initial_marking.size() != net.places.size()) ||
      (!options.final_marking.empty() && options.final_marking.size() != net.places.size()))
    return reject("invalid marking dimension or max_states");
  struct WeightedEndpoint { std::size_t place; TokenInteger magnitude; bool negative; };
  std::vector<std::vector<WeightedEndpoint>> inputs(net.transitions.size()), outputs(net.transitions.size());
  std::vector<char> has_preset(net.places.size(), 0);
  for (const auto& arc : net.arcs) {
    if (arc.source.kind == PetriNodeKind::kPlace)
      inputs[transitions.at(arc.target.id)].push_back({places.at(arc.source.id), arc.weight, arc.negative_weight});
    else {
      const auto place = places.at(arc.target.id);
      outputs[transitions.at(arc.source.id)].push_back({place, arc.weight, arc.negative_weight});
      has_preset[place] = 1;
    }
  }
  PresentedMarking start;
  if (options.presentation_initial_marking) {
    std::unordered_set<std::size_t> keys;
    for (const auto& entry : *options.presentation_initial_marking) {
      const auto place = places.find(entry.place_id);
      if (place == places.end()) return reject("presented marking refers to an absent place");
      if (!keys.insert(entry.place_id).second) return reject("duplicate presented marking key");
      start.emplace_back(place->second, entry.tokens);
    }
  } else if (!options.initial_marking.empty()) {
    for (std::size_t i = 0; i < options.initial_marking.size(); ++i)
      if (options.initial_marking[i]) start.emplace_back(i, options.initial_marking[i]);
  } else {
    for (std::size_t i = 0; i < net.places.size(); ++i)
      if (!has_preset[i]) start.emplace_back(i, 1);
  }
  const auto key = [](PresentedMarking marking) { std::sort(marking.begin(), marking.end()); return marking; };
  const auto tokens = [](const PresentedMarking& marking, std::size_t place) {
    const auto found = std::find_if(marking.begin(), marking.end(), [place](const auto& entry) { return entry.first == place; });
    return found == marking.end() ? std::int64_t{0} : found->second;
  };
  const auto assign = [](PresentedMarking& marking, std::size_t place, std::int64_t value, bool remove_nonpositive) {
    const auto found = std::find_if(marking.begin(), marking.end(), [place](const auto& entry) { return entry.first == place; });
    if (remove_nonpositive && value <= 0) { if (found != marking.end()) marking.erase(found); }
    else if (found == marking.end()) marking.emplace_back(place, value);
    else found->second = value;
  };
  const auto adjust = [](std::int64_t value, const TokenInteger& magnitude,
                          bool negative, std::int64_t& result) {
    return petri_adjust_tokens(value, magnitude, negative, result);
  };
  std::vector<PresentedMarking> states{start};
  std::map<PresentedMarking, std::size_t> ids{{key(start), 0}};
  std::vector<std::size_t> state_names;
  std::map<std::string, std::size_t> named_ids;
  std::vector<std::size_t> representatives;
  const auto name_state = [&](const PresentedMarking& marking, std::size_t index) {
    const auto [found, inserted] = named_ids.emplace(cyclomatic_marking_name(net, marking), named_ids.size());
    if (inserted) representatives.push_back(index);
    state_names.push_back(found->second);
  };
  try { name_state(start, 0); }
  catch (const std::invalid_argument& error) { return reject(error.what()); }
  std::queue<std::size_t> pending;
  pending.push(0);
  std::set<std::tuple<std::size_t, std::string, std::size_t>> arcs;
  double scalar = 0.0;
  while (!pending.empty()) {
    const auto source = pending.front(); pending.pop();
    std::set<std::string> enabled_names;
    for (std::size_t t = 0; t < net.transitions.size(); ++t) {
      if (std::any_of(inputs[t].begin(), inputs[t].end(), [&](const auto& arc) {
            const auto value = tokens(states[source], arc.place);
            if (arc.negative) {
              if (arc.magnitude >= (std::uint64_t{1} << 63)) return false;
              return value < -static_cast<std::int64_t>(arc.magnitude);
            }
            return value < 0 || std::uint64_t(value) < arc.magnitude;
          })) continue;
      enabled_names.insert(net.transitions[t].name);
      auto next = states[source];
      // Public reachability uses weak firing: consumed places are removed when
      // depleted, including oversubscription by separately enabled input arcs.
      for (const auto& arc : inputs[t]) {
        std::int64_t following;
        if (!adjust(tokens(next, arc.place), arc.magnitude, !arc.negative, following))
          return reject("presented firing exceeds signed reachability range");
        assign(next, arc.place, following, true);
      }
      for (const auto& arc : outputs[t]) {
        std::int64_t following;
        if (!adjust(tokens(next, arc.place), arc.magnitude, arc.negative, following))
          return reject("presented firing exceeds signed reachability range");
        assign(next, arc.place, following, false);
      }
      const auto [found, inserted] = ids.emplace(key(next), states.size());
      if (inserted) {
        if (states.size() >= options.max_states) {
          out.exhausted = true;
          return reject("reachable-marking state cap exceeded");
        }
        try { name_state(next, states.size()); }
        catch (const std::invalid_argument& error) { return reject(error.what()); }
        states.push_back(std::move(next));
        pending.push(found->second);
      }
      arcs.emplace(state_names[source], net.transitions[t].name, state_names[found->second]);
    }
    // Equal displayed names select the first discovered representative. The
    // ordered native traversal is deterministic; public set-order gates remain.
    if (representatives[state_names[source]] == source) scalar += double(enabled_names.size());
  }
  std::vector<std::vector<std::size_t>> adjacency(named_ids.size());
  for (const auto& [source, transition, target] : arcs) adjacency[source].push_back(target);
  finish_cyclomatic(out, adjacency, scalar);
  return out;
}

} // namespace

SimplicityResult evaluate_simplicity(const PetriNet& net,
                                     SimplicityVariant variant,
                                     const SimplicityOptions& options) {
  if (variant == SimplicityVariant::EXTENDED_CYCLOMATIC &&
      (net.has_extended_arcs() || net.has_compact_ordinary_arcs())) {
    PnmlDocument projection;
    projection.net = net;
    return evaluate_simplicity(projection, variant, options);
  }
  SimplicityResult out;
  out.variant = variant;
  if (variant == SimplicityVariant::ARC_DEGREE) {
    std::size_t arcs = 0;
    for (const auto& transition : net.transitions()) {
      const std::unordered_set<std::size_t> inputs(
          transition.inputs.begin(), transition.inputs.end());
      const std::unordered_set<std::size_t> reset_inputs(
          transition.reset_inputs.begin(), transition.reset_inputs.end());
      const std::unordered_set<std::size_t> inhibitor_inputs(
          transition.inhibitor_inputs.begin(),
          transition.inhibitor_inputs.end());
      const std::unordered_set<std::size_t> outputs(
          transition.outputs.begin(), transition.outputs.end());
      arcs += inputs.size() + reset_inputs.size() + inhibitor_inputs.size() +
              outputs.size();
    }
    out.vertices = net.place_count() + net.transitions().size();
    const double mean_degree = out.vertices
                                   ? 2.0 * double(arcs) / double(out.vertices)
                                   : 0.0;
    out.complexity = std::max(mean_degree - options.arc_degree_k, 0.0);
    out.simplicity = 1.0 / (1.0 + out.complexity);
    out.value = out.simplicity;
    return out;
  }

  if (variant == SimplicityVariant::EXTENDED_CARDOSO) {
    std::size_t total = 0;
    for (std::size_t place = 0; place < net.place_count(); ++place) {
      std::set<std::vector<std::size_t>> downstream;
      for (const auto& transition : net.transitions()) {
        const auto contains_place = [place](const auto& inputs) {
          return std::find(inputs.begin(), inputs.end(), place) != inputs.end();
        };
        if (!contains_place(transition.inputs) &&
            !contains_place(transition.reset_inputs) &&
            !contains_place(transition.inhibitor_inputs))
          continue;
        auto places = transition.outputs;
        std::sort(places.begin(), places.end());
        places.erase(std::unique(places.begin(), places.end()), places.end());
        downstream.insert(std::move(places));
      }
      total += downstream.size();
    }
    out.vertices = net.place_count();
    out.complexity = out.value = double(total);
    out.simplicity = out.value;
    return out;
  }

  if (variant != SimplicityVariant::EXTENDED_CYCLOMATIC) {
    out.computed = false;
    out.error = "unsupported simplicity variant";
    return out;
  }

  if (options.max_states == 0 ||
      (!options.initial_marking.empty() &&
       options.initial_marking.size() != net.place_count()) ||
      (!options.final_marking.empty() &&
       options.final_marking.size() != net.place_count())) {
    out.computed = false;
    out.error = "invalid marking dimension or max_states";
    return out;
  }
  const auto has_negative = [](const Marking& marking) {
    return std::any_of(marking.begin(), marking.end(),
                       [](int tokens) { return tokens < 0; });
  };
  if (has_negative(options.initial_marking) ||
      has_negative(options.final_marking)) {
    out.computed = false;
    out.error = "negative marking";
    return out;
  }

  Marking start = initial(net, options);
  if (options.initial_marking.empty()) {
    start.assign(net.place_count(), 0);
    for (std::size_t place = 0; place < net.place_count(); ++place) {
      const bool has_preset = std::any_of(
          net.transitions().begin(), net.transitions().end(),
          [place](const PetriTransition& transition) {
            return std::find(transition.outputs.begin(),
                             transition.outputs.end(), place) !=
                   transition.outputs.end();
          });
      if (!has_preset) start[place] = 1;
    }
  }

  std::vector<Marking> states{start};
  std::queue<std::size_t> queue;
  queue.push(0);
  std::unordered_map<Marking, std::size_t, MarkingHash> ids{{start, 0}};
  std::vector<std::vector<std::size_t>> adjacency(1);
  while (!queue.empty()) {
    const auto source = queue.front();
    queue.pop();
    for (const auto& transition : net.transitions()) {
      if (!enabled(transition, states[source])) continue;
      auto next = fire(transition, states[source]);
      const auto [it, inserted] = ids.emplace(next, states.size());
      if (inserted) {
        if (states.size() >= options.max_states) {
          out.exhausted = true;
          out.computed = false;
          out.error = "reachable-marking state cap exceeded";
          return out;
        }
        states.push_back(std::move(next));
        adjacency.emplace_back();
        queue.push(it->second);
      }
      // Each enabled transition contributes its own reachable occurrence,
      // including transitions that reach the same marking.
      adjacency[source].push_back(it->second);
    }
  }

  finish_cyclomatic(out, adjacency);
  return out;
}

SimplicityResult evaluate_simplicity(const PetriNetPresentation& net,
                                     SimplicityVariant variant,
                                     const SimplicityOptions& options) {
  SimplicityResult out;
  out.variant = variant;
  const auto reject = [&](const char* error) {
    out.computed = false;
    out.error = error;
    return out;
  };
  if (variant != SimplicityVariant::ARC_DEGREE &&
      variant != SimplicityVariant::EXTENDED_CARDOSO &&
      variant != SimplicityVariant::EXTENDED_CYCLOMATIC)
    return reject("unsupported simplicity variant");

  std::unordered_map<std::size_t, std::size_t> places, transitions;
  for (std::size_t i = 0; i < net.places.size(); ++i)
    if (!places.emplace(net.places[i].id, i).second)
      return reject("duplicate presented place identity");
  for (std::size_t i = 0; i < net.transitions.size(); ++i)
    if (!transitions.emplace(net.transitions[i].id, i).second)
      return reject("duplicate presented transition identity");

  std::vector<std::set<std::size_t>> place_successors(net.places.size());
  std::vector<std::set<std::size_t>> transition_successors(net.transitions.size());
  for (const auto& arc : net.arcs) {
    if (arc.source.kind == PetriNodeKind::kPlace &&
        arc.target.kind == PetriNodeKind::kTransition) {
      const auto p = places.find(arc.source.id);
      const auto t = transitions.find(arc.target.id);
      if (p == places.end() || t == transitions.end())
        return reject("presented arc refers to an absent endpoint");
      place_successors[p->second].insert(t->second);
    } else if (arc.source.kind == PetriNodeKind::kTransition &&
               arc.target.kind == PetriNodeKind::kPlace) {
      const auto t = transitions.find(arc.source.id);
      const auto p = places.find(arc.target.id);
      if (t == transitions.end() || p == places.end())
        return reject("presented arc refers to an absent endpoint");
      transition_successors[t->second].insert(p->second);
    } else {
      return reject("presented arc endpoints must be a place and a transition");
    }
  }
  if (variant == SimplicityVariant::EXTENDED_CYCLOMATIC)
    return evaluate_presented_cyclomatic(net, places, transitions, options);
  out.edges = net.arcs.size();
  if (variant == SimplicityVariant::ARC_DEGREE) {
    out.vertices = net.places.size() + net.transitions.size();
    const double mean_degree = out.vertices
        ? 2.0 * double(out.edges) / double(out.vertices) : 0.0;
    out.complexity = std::max(mean_degree - options.arc_degree_k, 0.0);
    out.simplicity = out.value = 1.0 / (1.0 + out.complexity);
  } else {
    std::size_t total = 0;
    for (const auto& successors : place_successors) {
      std::set<std::set<std::size_t>> choices;
      for (const auto transition : successors)
        choices.insert(transition_successors[transition]);
      total += choices.size();
    }
    out.vertices = net.places.size();
    out.complexity = out.simplicity = out.value = double(total);
  }
  return out;
}

SimplicityResult evaluate_simplicity(const PnmlDocument& document,
                                     SimplicityVariant variant,
                                     const SimplicityOptions& options) {
  try {
    return evaluate_simplicity(to_petri_net_presentation(document), variant, options);
  } catch (const std::invalid_argument& error) {
    SimplicityResult out;
    out.variant = variant;
    out.computed = false;
    out.error = error.what();
    return out;
  }
}

SimplicityResult evaluate_simplicity(const ProcessTree& tree,
                                     SimplicityVariant variant,
                                     const SimplicityOptions& options) {
  return evaluate_simplicity(to_petri_net(tree), variant, options);
}

namespace {

CompositeEvaluationResult evaluate_composite_impl(
    const EventLog& log, const PetriNet& net,
    const CompositeEvaluationOptions& options,
    const PnmlDocument* document) {
  enforce_evaluation_event_log_cap(log, options.max_rows_to_materialize);
  CompositeEvaluationResult out;
  out.fitness_details.variant = ReplayFitnessVariant::TOKEN_BASED;
  if (const auto error = validate_token_evaluation_markings(net, options);
      !error.empty()) {
    out.computed = false;
    out.error = error;
    out.fitness_details.computed = false;
    out.fitness_details.error = error;
    return out;
  }
  const double total = options.fitness_weight + options.precision_weight +
                       options.simplicity_weight + options.generalization_weight;
  if (total == 0.0) {
    out.computed = false;
    out.error = "composite weights must have a non-zero total";
    out.fitness_details.computed = false;
    out.fitness_details.error = out.error;
    return out;
  }
  out.fitness_weight = options.fitness_weight / total;
  out.precision_weight = options.precision_weight / total;
  out.simplicity_weight = options.simplicity_weight / total;
  out.generalization_weight = options.generalization_weight / total;

  const std::string activity_attribute = options.activity_attribute.empty()
                                             ? "concept:name"
                                             : options.activity_attribute;
  ReplayFitnessOptions fitness_options;
  fitness_options.activity_attribute = activity_attribute;
  fitness_options.case_id_attribute = options.case_id_attribute;
  fitness_options.max_rows_to_materialize = options.max_rows_to_materialize;
  copy_token_markings(options, fitness_options);
  PrecisionOptions precision_options;
  precision_options.activity_attribute = activity_attribute;
  precision_options.case_id_attribute = options.case_id_attribute;
  precision_options.max_rows_to_materialize = options.max_rows_to_materialize;
  copy_token_markings(options, precision_options);
  GeneralizationOptions generalization_options;
  generalization_options.activity_attribute = activity_attribute;
  generalization_options.case_id_attribute = options.case_id_attribute;
  generalization_options.max_rows_to_materialize =
      options.max_rows_to_materialize;
  copy_token_markings(options, generalization_options);
  const auto fitness = evaluate_replay_fitness(
      log, net, ReplayFitnessVariant::TOKEN_BASED, fitness_options);
  const auto precision = evaluate_precision(
      log, net, PrecisionVariant::ETCONFORMANCE_TOKEN, precision_options);
  const auto generalization = evaluate_generalization(
      log, net, GeneralizationVariant::GENERALIZATION_TOKEN,
      generalization_options);
  const auto simplicity = document
      ? evaluate_simplicity(*document, SimplicityVariant::ARC_DEGREE)
      : evaluate_simplicity(net, SimplicityVariant::ARC_DEGREE);
  out.fitness_details = fitness;
  out.fitness = fitness.log_fitness;
  out.fitness_score = out.fitness;
  out.precision = precision.precision;
  out.generalization = generalization.generalization;
  out.simplicity = simplicity.simplicity;
  out.metrics_average_weight =
      out.fitness_weight * out.fitness +
      out.precision_weight * out.precision +
      out.generalization_weight * out.generalization +
      out.simplicity_weight * out.simplicity;
  const double denominator = out.fitness + out.precision;
  out.fscore = denominator > 0.0
                   ? 2.0 * out.fitness * out.precision / denominator
                   : 0.0;
  if (!fitness.computed || !precision.computed || !generalization.computed ||
      !simplicity.computed) {
    out.computed = false;
    if (!fitness.error.empty()) out.error = fitness.error;
    else if (!precision.error.empty()) out.error = precision.error;
    else if (!generalization.error.empty()) out.error = generalization.error;
    else out.error = simplicity.error;
  }
  return out;
}

}  // namespace

CompositeEvaluationResult evaluate_composite(
    const EventLog& log, const PetriNet& net,
    const CompositeEvaluationOptions& options) {
  return evaluate_composite_impl(log, net, options, nullptr);
}

CompositeEvaluationResult evaluate_composite(
    const EventLog& log, const PnmlDocument& document,
    const CompositeEvaluationOptions& options) {
  auto selected = options;
  if (!inherit_document_token_markings(document, selected)) {
    CompositeEvaluationResult out;
    out.computed = out.fitness_details.computed = false;
    out.error = out.fitness_details.error = "PNML marking dimension does not match net";
    return out;
  }
  return evaluate_composite_impl(log, document.net, selected, &document);
}

CompositeEvaluationResult evaluate_composite(
    const EventLog& log, const ProcessTree& tree,
    const CompositeEvaluationOptions& options) {
  return evaluate_composite(log, to_petri_net(tree), options);
}

CompositeEvaluationResult evaluate_composite(
    const EventDataView& data, const PetriNet& net,
    const CompositeEvaluationOptions& options) {
  return evaluate_composite(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      net, options);
}

CompositeEvaluationResult evaluate_composite(
    const EventDataView& data, const ProcessTree& tree,
    const CompositeEvaluationOptions& options) {
  return evaluate_composite(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      tree, options);
}

CompositeEvaluationResult evaluate_composite(
    const EventDataView& data, const PnmlDocument& document,
    const CompositeEvaluationOptions& options) {
  return evaluate_composite(
      evaluation_event_log(data, options.activity_attribute,
                           options.case_id_attribute,
                           options.max_rows_to_materialize),
      document, options);
}

StochasticLanguage derive_stochastic_language(const EventLog& log,
                                              bool normalize) {
  StochasticLanguage out;
  std::map<std::vector<std::string>, double> counts;
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events) activities.push_back(event.activity);
    counts[std::move(activities)] += 1.0;
  }
  for (auto& [activities, weight] : counts) {
    out.traces.push_back({activities, weight});
    out.total_mass += weight;
  }
  if (normalize && out.total_mass > 0.0) {
    for (auto& trace : out.traces) trace.weight /= out.total_mass;
    out.total_mass = 1.0;
  }
  return out;
}

namespace {

using TraceKey = std::vector<std::string>;
using TraceMasses = std::map<TraceKey, double>;

bool forbidden_language_codepoint(char32_t codepoint) {
  // These symbols have special meaning to the regular-expression utilities
  // that consume encoded traces. Keep the assignment deterministic while
  // allowing a full Unicode code point per activity.
  switch (codepoint) {
    case U'[':
    case U']':
    case U'(':
    case U')':
    case U'*':
    case U'+':
    case U'^':
    case U'?':
    case U'\r':
    case U'\n':
    case U' ':
    case U'\t':
    case U'$':
    case U'"':
    case U'!':
    case U'#':
    case U'&':
    case U'%':
    case U'|':
    case U'.':
    case U',':
    case U';':
    case U'-':
    case U'\'':
    case U'\\':
    case U'/':
    case U'{':
    case U'}':
      return true;
    default:
      return false;
  }
}

std::map<std::string, char32_t> encode_activity_union(
    const TraceMasses& left, const TraceMasses& right) {
  std::set<std::string> activities;
  for (const auto& [trace, _] : left)
    activities.insert(trace.begin(), trace.end());
  for (const auto& [trace, _] : right)
    activities.insert(trace.begin(), trace.end());

  std::map<std::string, char32_t> encoded;
  char32_t next = 1;
  for (const auto& activity : activities) {
    while (forbidden_language_codepoint(next) ||
           (next >= 0xD800 && next <= 0xDFFF)) {
      ++next;
    }
    if (next > 0x10FFFF)
      throw std::length_error("stochastic language alphabet is too large");
    encoded.emplace(activity, next++);
  }
  return encoded;
}

std::u32string encode_trace(const TraceKey& trace,
                            const std::map<std::string, char32_t>& alphabet) {
  std::u32string encoded;
  encoded.reserve(trace.size());
  for (const auto& activity : trace) encoded.push_back(alphabet.at(activity));
  return encoded;
}

std::string utf8_from_encoded(const std::u32string& value) {
  std::string out;
  for (const char32_t codepoint : value) {
    if (codepoint <= 0x7F) {
      out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }
  return out;
}

bool consolidate_language(const StochasticLanguage& input,
                          TraceMasses& output, double& total_mass) {
  output.clear();
  for (const auto& trace : input.traces) {
    if (!std::isfinite(trace.weight) || trace.weight < 0.0) return false;
    auto& total = output[trace.activities];
    total += trace.weight;
    if (!std::isfinite(total)) return false;
  }
  total_mass = 0.0;
  for (const auto& [_, weight] : output) {
    total_mass += weight;
    if (!std::isfinite(total_mass)) return false;
  }
  return true;
}

double encoded_normalized_levenshtein(const std::u32string& left,
                                       const std::u32string& right) {
  if (left.empty() && right.empty()) return 0.0;
  std::vector<double> distance(right.size() + 1);
  std::iota(distance.begin(), distance.end(), 0.0);
  for (std::size_t i = 1; i <= left.size(); ++i) {
    double previous = distance[0];
    distance[0] = static_cast<double>(i);
    for (std::size_t j = 1; j <= right.size(); ++j) {
      const double saved = distance[j];
      distance[j] = std::min(
          {distance[j] + 1.0, distance[j - 1] + 1.0,
           previous + (left[i - 1] == right[j - 1] ? 0.0 : 1.0)});
      previous = saved;
    }
  }
  return distance.back() /
         static_cast<double>(std::max(left.size(), right.size()));
}

}  // namespace

double normalized_levenshtein_distance(const std::u32string& left,
                                       const std::u32string& right) {
  return encoded_normalized_levenshtein(left, right);
}

EmdResult evaluate_emd(const StochasticLanguage& left_input,
                       const StochasticLanguage& right_input,
                       const EmdOptions& options) {
  EmdResult out;
  out.variant = options.variant;
  out.use_fast_emd = options.use_fast_emd;
  switch (options.variant) {
    case EmdVariant::EXACT:
    case EmdVariant::PYEMD:
      break;
    default:
      out.computed = false;
      out.error = "unsupported EMD variant";
      return out;
  }

  TraceMasses left;
  TraceMasses right;
  double left_mass = 0.0;
  double right_mass = 0.0;
  if (!consolidate_language(left_input, left, left_mass) ||
      !consolidate_language(right_input, right, right_mass)) {
    out.computed = false;
    out.error = "invalid language weight";
    return out;
  }

  // A language with only zero-weight entries is still part of the alphabet,
  // but carries no transport. Two such languages therefore have distance
  // zero, including when their trace keys differ.
  if (left_mass == 0.0 && right_mass == 0.0) {
    out.distance = 0.0;
    return out;
  }
  if (left_mass <= 0.0 || right_mass <= 0.0) {
    out.computed = false;
    out.error = "stochastic language has zero mass";
    return out;
  }

  const bool masses_close =
      std::abs(left_mass - right_mass) <=
      1e-8 + 1e-5 * std::abs(right_mass);
  if (!masses_close) {
    // Strict mode is checked before normalization so callers can detect an
    // unequal input even though the accelerated path normally rescales it.
    if (options.strict_equal_mass) {
      out.computed = false;
      out.error = "masses must be equal";
      return out;
    }
    // The fallback implementation has equality constraints. The accelerated
    // path is the only one that accepts unequal masses by normalization.
    if (options.variant == EmdVariant::PYEMD && !options.use_fast_emd) {
      out.computed = false;
      out.error = "masses must be equal";
      return out;
    }
    if (!options.normalize_masses) {
      out.computed = false;
      out.error = "masses must be equal";
      return out;
    }
    for (auto& [_, weight] : left) weight /= left_mass;
    for (auto& [_, weight] : right) weight /= right_mass;
    left_mass = right_mass = 1.0;
    out.normalized = true;
  }

  // Ensure every trace key occurs in both histograms. This is important for
  // encoded callbacks: a distance callback sees the same deterministic union
  // alphabet regardless of which side introduced a trace.
  for (const auto& [trace, _] : left) right.try_emplace(trace, 0.0);
  for (const auto& [trace, _] : right) left.try_emplace(trace, 0.0);
  const auto alphabet = encode_activity_union(left, right);

  std::vector<TraceKey> left_keys;
  std::vector<TraceKey> right_keys;
  std::vector<double> left_weights;
  std::vector<double> right_weights;
  left_keys.reserve(left.size());
  right_keys.reserve(right.size());
  left_weights.reserve(left.size());
  right_weights.reserve(right.size());
  std::vector<std::u32string> left_encoded;
  std::vector<std::u32string> right_encoded;
  left_encoded.reserve(left.size());
  right_encoded.reserve(right.size());
  for (const auto& [trace, weight] : left) {
    left_keys.push_back(trace);
    left_weights.push_back(weight);
    left_encoded.push_back(encode_trace(trace, alphabet));
  }
  for (const auto& [trace, weight] : right) {
    right_keys.push_back(trace);
    right_weights.push_back(weight);
    right_encoded.push_back(encode_trace(trace, alphabet));
  }

  const auto distance = [&](std::size_t i, std::size_t j) {
    if (options.encoded_distance) {
      return options.encoded_distance(left_encoded[i], right_encoded[j]);
    }
    if (options.string_distance) {
      return options.string_distance(left_encoded[i], right_encoded[j]);
    }
    if (options.utf8_string_distance) {
      return options.utf8_string_distance(
          utf8_from_encoded(left_encoded[i]),
          utf8_from_encoded(right_encoded[j]));
    }
    if (options.distance) {
      return options.distance(left_keys[i], right_keys[j]);
    }
    return encoded_normalized_levenshtein(left_encoded[i], right_encoded[j]);
  };

  // The reference PYEMD cost divides the Levenshtein distance by the longer
  // trace length, so an empty trace in either language (both histograms share
  // the union of trace keys) makes it raise ZeroDivisionError.
  const bool default_distance = !options.encoded_distance &&
                                !options.string_distance &&
                                !options.utf8_string_distance &&
                                !options.distance;
  if (options.variant == EmdVariant::PYEMD && default_distance &&
      std::any_of(left_encoded.begin(), left_encoded.end(),
                  [](const std::u32string& trace) { return trace.empty(); })) {
    out.computed = false;
    out.error =
        "normalized Levenshtein distance is undefined for two empty traces";
    return out;
  }

  const std::size_t left_size = left_keys.size();
  const std::size_t right_size = right_keys.size();
  std::vector<std::vector<double>> costs(
      left_size, std::vector<double>(right_size, 0.0));
  try {
    for (std::size_t i = 0; i < left_size; ++i) {
      for (std::size_t j = 0; j < right_size; ++j) {
        costs[i][j] = distance(i, j);
        if (!std::isfinite(costs[i][j]) || costs[i][j] < 0.0) {
          out.computed = false;
          out.error = "invalid distance";
          return out;
        }
      }
    }
  } catch (const std::exception& error) {
    out.computed = false;
    out.error = std::string("distance callback failed: ") + error.what();
    return out;
  } catch (...) {
    out.computed = false;
    out.error = "distance callback failed";
    return out;
  }

  struct Edge {
    int to = 0;
    int reverse = 0;
    double capacity = 0.0;
    double cost = 0.0;
  };
  const int source = 0;
  const int left_begin = 1;
  const int right_begin = left_begin + static_cast<int>(left_size);
  const int sink = right_begin + static_cast<int>(right_size);
  const int node_count = sink + 1;
  std::vector<std::vector<Edge>> graph(node_count);
  const auto add_edge = [&](int from, int to, double capacity, double cost) {
    graph[from].push_back(
        {to, static_cast<int>(graph[to].size()), capacity, cost});
    graph[to].push_back({from, static_cast<int>(graph[from].size() - 1), 0.0,
                         -cost});
  };
  for (std::size_t i = 0; i < left_size; ++i)
    add_edge(source, left_begin + static_cast<int>(i), left_weights[i], 0.0);
  for (std::size_t j = 0; j < right_size; ++j)
    add_edge(right_begin + static_cast<int>(j), sink, right_weights[j], 0.0);
  for (std::size_t i = 0; i < left_size; ++i) {
    for (std::size_t j = 0; j < right_size; ++j) {
      add_edge(left_begin + static_cast<int>(i),
               right_begin + static_cast<int>(j),
               std::numeric_limits<double>::infinity(), costs[i][j]);
    }
  }

  const double target = std::min(left_mass, right_mass);
  constexpr double kDistanceEpsilon = 1e-12;
  const double flow_epsilon =
      64.0 * std::numeric_limits<double>::epsilon() * target;
  double flow = 0.0;
  double total_cost = 0.0;
  while (flow + flow_epsilon < target) {
    std::vector<double> distances(
        node_count, std::numeric_limits<double>::infinity());
    std::vector<int> previous_node(node_count, -1);
    std::vector<int> previous_edge(node_count, -1);
    distances[source] = 0.0;
    for (int iteration = 0; iteration < node_count - 1; ++iteration) {
      bool changed = false;
      for (int from = 0; from < node_count; ++from) {
        if (!std::isfinite(distances[from])) continue;
        for (std::size_t edge = 0; edge < graph[from].size(); ++edge) {
          const auto& candidate = graph[from][edge];
          if (candidate.capacity <= flow_epsilon) continue;
          if (distances[candidate.to] >
              distances[from] + candidate.cost + kDistanceEpsilon) {
            distances[candidate.to] = distances[from] + candidate.cost;
            previous_node[candidate.to] = from;
            previous_edge[candidate.to] = static_cast<int>(edge);
            changed = true;
          }
        }
      }
      if (!changed) break;
    }
    if (previous_node[sink] < 0) {
      out.computed = false;
      out.error = "transportation solve failed";
      return out;
    }
    double amount = target - flow;
    for (int node = sink; node != source; node = previous_node[node]) {
      const auto& edge = graph[previous_node[node]][previous_edge[node]];
      amount = std::min(amount, edge.capacity);
    }
    if (!(amount > flow_epsilon) || !std::isfinite(amount)) {
      out.computed = false;
      out.error = "transportation solve failed";
      return out;
    }
    for (int node = sink; node != source; node = previous_node[node]) {
      auto& edge = graph[previous_node[node]][previous_edge[node]];
      edge.capacity -= amount;
      graph[node][edge.reverse].capacity += amount;
    }
    const double cost_increment = amount * distances[sink];
    const double next_total_cost = total_cost + cost_increment;
    if (!std::isfinite(cost_increment) || !std::isfinite(next_total_cost)) {
      out.computed = false;
      out.error = "transportation cost overflow";
      return out;
    }
    flow += amount;
    total_cost = next_total_cost;
  }
  out.distance = total_cost;
  return out;
}

double earth_movers_distance(const StochasticLanguage& left,
                             const StochasticLanguage& right,
                             const EmdOptions& options) {
  const auto result = evaluate_emd(left, right, options);
  if (!result.computed) throw std::invalid_argument(result.error);
  return result.distance;
}

} // namespace minera
