#include "minera/quality_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
namespace minera {
namespace {
[[noreturn]] void invalid(std::string_view message) {
  throw std::invalid_argument("quality workbench JSON: " + std::string(message));
}
std::string q(std::string_view value) { return serialize_embedding_workbench_string_json(value); }
std::ostringstream output() { std::ostringstream out; out.imbue(std::locale::classic()); return out; }

// A bounded structural index into submitted JSON. Native string/float parsing
// delegates to the existing public codecs; this index never parses output.
struct Json {
  enum class Kind { object, array, string, number, boolean, null } kind;
  std::string_view raw;
  std::map<std::string, Json> fields;
  std::vector<Json> items;
};
std::string string_token(std::string_view raw) {
  auto values = parse_literal_string_vector_json("[" + std::string(raw) + "]");
  if (values.size() != 1) invalid("expected one JSON string");
  return std::move(values.front());
}
class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {
    if (text.size() > 32u * 1024u * 1024u) invalid("request exceeds 32 MiB");
  }
  Json parse() { auto result = value(0); whitespace(); if (at_ != text_.size()) invalid("data after JSON value"); return result; }
 private:
  std::string_view text_; std::size_t at_ = 0, nodes_ = 0;
  void whitespace() { while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\r' || text_[at_] == '\n')) ++at_; }
  bool take(char c) { whitespace(); if (at_ < text_.size() && text_[at_] == c) { ++at_; return true; } return false; }
  std::string_view quoted() {
    whitespace(); const auto begin = at_;
    if (at_ == text_.size() || text_[at_++] != '"') invalid("expected JSON string");
    bool closed = false;
    while (at_ < text_.size()) {
      const auto c = text_[at_++];
      if (c == '"') { closed = true; break; }
      if (c == '\\') { if (at_ == text_.size()) invalid("short string escape"); ++at_; }
    }
    if (!closed) invalid("unterminated string");
    const auto token = text_.substr(begin, at_ - begin); (void)string_token(token); return token;
  }
  Json value(unsigned depth) {
    whitespace(); if (depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000) invalid("request exceeds one million values");
    if (at_ == text_.size()) invalid("missing JSON value");
    const auto begin = at_; Json result{Json::Kind::null, {}, {}, {}};
    if (text_[at_] == '{') {
      result.kind = Json::Kind::object; ++at_;
      if (!take('}')) do {
        const auto key = string_token(quoted()); if (!take(':')) invalid("missing object colon");
        if (!result.fields.emplace(key, value(depth + 1)).second) invalid("duplicate object member");
        if (take('}')) break;
        if (!take(',')) invalid("missing object comma");
      } while (true);
    } else if (text_[at_] == '[') {
      result.kind = Json::Kind::array; ++at_;
      if (!take(']')) do {
        result.items.push_back(value(depth + 1)); if (take(']')) break;
        if (!take(',')) invalid("missing array comma");
      } while (true);
    } else if (text_[at_] == '"') { result.kind = Json::Kind::string; (void)quoted(); }
    else if (text_.substr(at_, 4) == "true" || text_.substr(at_, 5) == "false") {
      result.kind = Json::Kind::boolean; at_ += text_[at_] == 't' ? 4 : 5;
    } else if (text_.substr(at_, 4) == "null") { at_ += 4; }
    else {
      result.kind = Json::Kind::number;
      if (text_[at_] == '-') ++at_;
      if (at_ == text_.size()) invalid("short number");
      if (text_[at_] == '0') ++at_;
      else { if (text_[at_] < '1' || text_[at_] > '9') invalid("invalid JSON value"); while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_; }
      if (at_ < text_.size() && text_[at_] == '.') {
        const auto digits = ++at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_;
        if (digits == at_) invalid("missing fractional digits");
      }
      if (at_ < text_.size() && (text_[at_] == 'e' || text_[at_] == 'E')) {
        ++at_; if (at_ < text_.size() && (text_[at_] == '+' || text_[at_] == '-')) ++at_;
        const auto digits = at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_;
        if (digits == at_) invalid("missing exponent digits");
      }
    }
    result.raw = text_.substr(begin, at_ - begin); return result;
  }
};
const Json* field(const Json& value, std::string_view name) {
  const auto found = value.fields.find(std::string(name)); return found == value.fields.end() ? nullptr : &found->second;
}
template<class T> T integer(const Json& value) {
  if (value.kind != Json::Kind::number || value.raw.empty()) invalid("expected an integer JSON token");
  auto digits = value.raw;
  if constexpr (std::is_signed_v<T>) { if (digits.front() == '-') digits.remove_prefix(1); }
  if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) invalid("expected an integer JSON token");
  T result{}; const auto parsed = std::from_chars(value.raw.data(), value.raw.data() + value.raw.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.raw.data() + value.raw.size()) invalid("integer exceeds native storage range");
  return result;
}

void object(const Json& value) { if(value.kind!=Json::Kind::object) invalid("expected object"); }
void allowed(const Json& value,std::initializer_list<std::string_view> names) {
  object(value);for(const auto& [name,item]:value.fields){(void)item;if(std::find(names.begin(),names.end(),name)==names.end())invalid("unknown field "+name);}
}
const Json& required(const Json& value,std::string_view name){object(value);const auto* item=field(value,name);if(!item)invalid("missing field "+std::string(name));return *item;}
std::string native_string(const Json& value){return string_token(value.raw);}
bool boolean(const Json& value){if(value.kind!=Json::Kind::boolean)invalid("expected boolean");return value.raw=="true";}


template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{using Item=T;};
template<class T>struct Vector:std::false_type{};
template<class T>struct Vector<std::vector<T>>:std::true_type{using Item=T;};
// TokenMarking is a public vector-derived carrier, not a std::vector alias.
template<>struct Vector<TokenMarking>:std::true_type{using Item=TokenInteger;};
template<class T>struct Fixed:std::false_type{};
template<class T,std::size_t N>struct Fixed<std::array<T,N>>:std::true_type{using Item=T;static constexpr auto size=N;};
template<class T>struct Map:std::false_type{};
template<class K,class V,class C,class A>struct Map<std::map<K,V,C,A>>:std::true_type{using Key=K;using Item=V;};
template<class K,class V,class H,class E,class A>struct Map<std::unordered_map<K,V,H,E,A>>:std::true_type{using Key=K;using Item=V;};
template<class T>struct Variant:std::false_type{};
template<class...T>struct Variant<std::variant<T...>>:std::true_type{};
using CostFunction=std::function<double(const std::string&,std::size_t)>;
template<class T,class M>struct Member{std::string_view name;M T::*pointer;};
template<class T,class M>auto member(std::string_view name,M T::*pointer){return Member<T,M>{name,pointer};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(ReplayFitnessOptions,F(consider_activities_not_in_model_in_fitness),F(activity_attribute),F(token_replay_variant),F(alignment_options),F(decomposed_alignment_options),F(initial_marking),F(final_marking),F(case_id_attribute),F(max_rows_to_materialize),F(cleaning_token_flood),F(exact_initial_marking),F(exact_final_marking),F(initial_marking_keys),F(final_marking_keys),F(place_names),F(transition_names))
RECORD(PrecisionOptions,F(use_task_ids),F(max_states),F(activity_attribute),F(token_replay_variant),F(initial_marking),F(final_marking),F(case_id_attribute),F(max_rows_to_materialize),F(exact_initial_marking),F(exact_final_marking),F(initial_marking_keys),F(final_marking_keys),F(place_names),F(transition_names))
RECORD(GeneralizationOptions,F(activity_attribute),F(initial_marking),F(final_marking),F(case_id_attribute),F(max_rows_to_materialize),F(exact_initial_marking),F(exact_final_marking),F(initial_marking_keys),F(final_marking_keys),F(place_names),F(transition_names))
RECORD(SimplicityOptions,F(initial_marking),F(final_marking),F(max_states),F(arc_degree_k),F(presentation_initial_marking))
RECORD(CompositeEvaluationOptions,F(fitness_weight),F(precision_weight),F(simplicity_weight),F(generalization_weight),F(activity_attribute),F(initial_marking),F(final_marking),F(case_id_attribute),F(max_rows_to_materialize),F(exact_initial_marking),F(exact_final_marking),F(initial_marking_keys),F(final_marking_keys),F(place_names),F(transition_names))
RECORD(ReplayFitnessResult,F(variant),F(fitness),F(log_fitness),F(average_trace_fitness),F(percentage_fit_traces),F(fitting_traces),F(trace_count),F(total_cost),F(computed),F(exhausted),F(error),F(token_result),F(alignment_result),F(decomposed_alignment_result),F(log_fitness_is_integer_zero),F(token_result_is_fitness_only))
RECORD(PrecisionResult,F(variant),F(precision),F(computed),F(exhausted),F(error),F(token_result),F(alignment_result),F(alignment_detail_present),F(token_result_is_precision_only))
RECORD(GeneralizationResult,F(variant),F(generalization),F(executed_transitions),F(transition_count),F(transition_frequencies),F(computed),F(error))
RECORD(SimplicityResult,F(variant),F(simplicity),F(complexity),F(value),F(vertices),F(edges),F(strongly_connected_components),F(computed),F(exhausted),F(error))
RECORD(CompositeEvaluationResult,F(fitness),F(precision),F(generalization),F(simplicity),F(metrics_average_weight),F(fscore),F(fitness_weight),F(precision_weight),F(simplicity_weight),F(generalization_weight),F(computed),F(error),F(fitness_details),F(fitness_score))
RECORD(AlignmentOptions,F(variant),F(cost_profile),F(max_states),F(max_time_seconds),F(max_total_time_seconds),F(max_alignments),F(exponent),F(synchronous_dijkstra),F(default_trace_cost),F(default_model_cost),F(default_sync_cost),F(default_silent_cost),F(trace_costs),F(model_costs),F(sync_costs),F(trace_costs_by_position),F(model_costs_by_transition),F(sync_costs_by_transition),F(trace_cost_function),F(model_cost_function),F(sync_cost_function),F(activity_attribute),F(initial_marking),F(final_marking))
RECORD(AlignmentDiagnostics,F(visited_states),F(queued_states),F(traversed_arcs),F(lp_solved),F(closed_set_length),F(num_visited_markings))
RECORD(AlignmentMove,F(type),F(activity),F(transition))
RECORD(ProjectedAlignmentMove,F(activity),F(transition),F(marking_before),F(marking_after))
RECORD(VariantAlignment,F(activities),F(count),F(moves),F(cost),F(fitness),F(optimal_alignments),F(diagnostics),F(worst_cost),F(projected_model_moves),F(complete_moves))
RECORD(AlignmentResult,F(computed),F(exhausted),F(fitness),F(precision),F(fitting_traces),F(trace_count),F(total_cost),F(variant),F(diagnostics),F(variants),F(activity_attribute))
RECORD(VariantConformance,F(activities),F(count),F(fitness),F(null_activity_indices))
RECORD(ConformanceResult,F(fitness),F(precision),F(generalization),F(simplicity),F(fitting_traces),F(trace_count),F(produced),F(consumed),F(missing),F(remaining),F(transition_frequencies),F(variants),F(activity_attribute),F(trace_diagnostics),F(duration_diagnostics),F(root_cause_diagnostics))
RECORD(TokenReplayTraceDiagnostics,F(trace_index),F(case_id),F(trace_is_fit),F(trace_fitness),F(produced),F(consumed),F(missing),F(remaining),F(activated_transitions),F(transitions_with_problems),F(enabled_transitions),F(unknown_event_indices),F(unknown_activities),F(unknown_activity_is_null),F(reached_marking),F(places),F(replay_stopped),F(replayed_events),F(event_diagnostics),F(reached_marking_keys),F(cleaned_place_indices),F(marking_domain),F(reached_marking_order))
RECORD(TokenReplayPlaceDiagnostics,F(missing),F(remaining),F(consumed),F(produced))
RECORD(TokenReplayEventDiagnostics,F(event_index),F(activated_transitions),F(transitions_with_problems))
RECORD(TokenReplayMarkingDomain,F(places),F(detached_places))
RECORD(TokenReplayDurationComparison,F(fit_trace_indices),F(problem_trace_indices),F(fit_count),F(problem_count),F(fit_median_seconds),F(problem_median_seconds),F(relative_throughput))
RECORD(TokenReplayTransitionDurationDiagnostics,F(fit_trace_indices),F(problem_trace_indices),F(fit_count),F(problem_count),F(fit_median_seconds),F(problem_median_seconds),F(relative_throughput),F(transition_index))
RECORD(TokenReplayUnknownActivityDurationDiagnostics,F(fit_trace_indices),F(problem_trace_indices),F(fit_count),F(problem_count),F(fit_median_seconds),F(problem_median_seconds),F(relative_throughput),F(activity),F(activity_is_null))
RECORD(TokenReplayDurationDiagnostics,F(timestamp_attribute),F(transitions),F(unknown_activities))
RECORD(TokenReplayTransitionRootCauseDiagnostics,F(feature_names),F(classes),F(data),F(target),F(source_trace_indices),F(tree),F(fit_count),F(problem_count),F(transition_index))
RECORD(TokenReplayUnknownRootCauseDiagnostics,F(feature_names),F(classes),F(data),F(target),F(source_trace_indices),F(tree),F(fit_count),F(problem_count),F(activity),F(activity_is_null),F(activity_attribute))
RECORD(TokenReplayRootCauseDiagnostics,F(options),F(transitions),F(unknown_activities))
RECORD(DecomposedAlignmentOptions,F(variant),F(threshold_border_agreement),F(max_time_trace),F(max_time_total),F(max_states),F(max_total_states),F(use_cache),F(clear_cache),F(log_move_cost),F(model_move_cost),F(tau_model_cost),F(sync_cost),F(activity_log_costs),F(activity_model_costs),F(activity_sync_costs),F(initial_marking),F(final_marking),F(activity_attribute))
RECORD(DecomposedAlignmentMove,F(log),F(model),F(transition))
RECORD(DecomposedAlignmentTrace,F(activities),F(count),F(complete),F(timed_out),F(exhausted),F(cost),F(fitness),F(best_worst_cost),F(alignment))
RECORD(DecomposedAlignmentDiagnostics,F(decompositions),F(components),F(merges),F(cache_hits),F(cache_misses),F(border_disagreements),F(agreement_iterations),F(visited_states),F(queued_states),F(traversed_arcs),F(whole_net_fallback),F(note))
RECORD(DecomposedAlignmentResult,F(complete),F(timed_out),F(exhausted),F(fitness),F(best_worst_cost),F(trace_count),F(total_cost),F(diagnostics),F(traces))
RECORD(PetriNetLiteralStorage,F(place_count),F(source_storage),F(sink_storage),F(has_source),F(has_sink),F(transitions),F(activities),F(discovery_markings),F(transition_names))
RECORD(PetriTransition,F(activity),F(inputs),F(outputs),F(reset_inputs),F(inhibitor_inputs),F(reset_weights),F(inhibitor_weights),F(input_weights),F(output_weights))
RECORD(PetriArcWeight,F(magnitude),F(negative))
RECORD(PetriNetPresentation,F(name),F(properties),F(places),F(transitions),F(arcs),F(initial_marking),F(final_marking))
RECORD(PetriPresentedPlace,F(id),F(name),F(properties))
RECORD(PetriPresentedTransition,F(id),F(name),F(label),F(properties))
RECORD(PetriNodeRef,F(kind),F(id))
RECORD(PetriPresentedArc,F(source),F(target),F(weight),F(properties),F(negative_weight))
RECORD(PetriSparseMarkingEntry,F(place_id),F(tokens))
RECORD(PetriDiscoveryMarkings,F(detached_places),F(initial),F(final))
RECORD(PetriMarkingPlace,F(kind),F(id))
RECORD(PetriMarkingEntry,F(place),F(tokens))
RECORD(PetriDetachedMarkingPlace,F(id),F(name))
RECORD(PnmlDocument,F(net),F(initial_marking),F(final_marking),F(stochastic_map),F(place_ids),F(transition_ids),F(initial_marking_keys),F(final_marking_keys),F(arc_occurrences))
RECORD(PnmlArcOccurrence,F(place),F(transition),F(input),F(type),F(weight),F(id),F(negative_weight))
RECORD(StochasticMap,F(transitions))
RECORD(StochasticMapTransition,F(transition),F(timing),F(distribution),F(parameter),F(secondary_parameter),F(tertiary_parameter),F(activation_weight),F(priority))
RECORD(EventStream,F(attributes),F(events),F(metadata))
RECORD(EventRecord,F(fields))
RECORD(EventTableIndex,F(kind),F(dtype),F(names),F(labels))
RECORD(EventTableCategoricalDtype,F(categories_dtype),F(categories),F(ordered))
RECORD(RootCauseOptions,F(string_attributes),F(numeric_attributes),F(enable_multiplier))
RECORD(RootCauseRepresentation,F(feature_names),F(data),F(target),F(source_trace_indices))
RECORD(RootCauseModel,F(feature_names),F(classes),F(data),F(target),F(source_trace_indices),F(tree),F(fit_count),F(problem_count))
RECORD(RootCauseTreeNode,F(feature_index),F(threshold),F(left),F(right),F(class_counts),F(missing_go_to_left),F(missing_only))
RECORD(ProcessTreeNode,F(op),F(activity),F(children))
RECORD(LogExtension,F(name),F(prefix),F(uri))
RECORD(LogClassifier,F(name),F(keys))
RECORD(LogGlobal,F(scope),F(attributes))
RECORD(EventDataMetadata,F(extensions),F(classifiers),F(trace_globals),F(event_globals),F(properties),F(globals),F(compatibility_snapshot_valid),F(compatibility_globals_snapshot),F(compatibility_trace_globals_snapshot),F(compatibility_event_globals_snapshot))
#undef RECORD
#undef F
template<class T>concept HasRecord=requires{Record<T>::members();};
template<class T>struct EnumNames;
template<>struct EnumNames<ReplayFitnessVariant>{static constexpr std::array<std::string_view,3> names={"AUTO","TOKEN_BASED","ALIGNMENT_BASED"};};
template<>struct EnumNames<PrecisionVariant>{static constexpr std::array<std::string_view,4> names={"AUTO","ETCONFORMANCE_TOKEN","ALIGN_ETCONFORMANCE","AUTOMATON_AFTER_ALIGN"};};
template<>struct EnumNames<GeneralizationVariant>{static constexpr std::array<std::string_view,1> names={"GENERALIZATION_TOKEN"};};
template<>struct EnumNames<SimplicityVariant>{static constexpr std::array<std::string_view,3> names={"ARC_DEGREE","EXTENDED_CARDOSO","EXTENDED_CYCLOMATIC"};};
template<>struct EnumNames<TokenReplayVariant>{static constexpr std::array<std::string_view,2> names={"FORWARD","BACKWARDS"};};
template<>struct EnumNames<AlignmentVariant>{static constexpr std::array<std::string_view,6> names={"STATE_EQUATION_A_STAR","DIJKSTRA_NO_HEURISTICS","DIJKSTRA_LESS_MEMORY","DISCOUNTED_A_STAR","GENERATOR_DIJKSTRA_NO_HEURISTICS","GENERATOR_DIJKSTRA_LESS_MEMORY"};};
template<>struct EnumNames<AlignmentCostProfile>{static constexpr std::array<std::string_view,2> names={"LEGACY_UNIT","STANDARD"};};
template<>struct EnumNames<DecomposedAlignmentVariant>{static constexpr std::array<std::string_view,1> names={"RECOMPOS_MAXIMAL"};};
template<>struct EnumNames<ProcessTreeOperator>{static constexpr std::array<std::string_view,8> names={"ACTIVITY","TAU","SEQUENCE","EXCLUSIVE","INCLUSIVE_CHOICE","PARALLEL","INTERLEAVING","LOOP"};};
template<>struct EnumNames<PetriArcType>{static constexpr std::array<std::string_view,3> names={"NORMAL","RESET","INHIBITOR"};};
template<>struct EnumNames<PetriNodeKind>{static constexpr std::array<std::string_view,2> names={"PLACE","TRANSITION"};};
template<>struct EnumNames<PetriMarkingPlaceKind>{static constexpr std::array<std::string_view,2> names={"ROOT","DETACHED"};};
template<>struct EnumNames<StochasticMapTiming>{static constexpr std::array<std::string_view,2> names={"TIMED","IMMEDIATE"};};
template<>struct EnumNames<StochasticMapDistribution>{static constexpr std::array<std::string_view,8> names={"DETERMINISTIC","EXPONENTIAL","CONSTANT0","IMMEDIATE","NORMAL","UNIFORM","LOGNORMAL","GAMMA"};};
template<>struct EnumNames<AlignmentMoveType>{static constexpr std::array<std::string_view,3> names={"SYNCHRONOUS","LOG","MODEL"};};
template<class T>std::string_view enum_name(T value){const auto i=static_cast<int>(value);const auto& names=EnumNames<T>::names;return i>=0&&static_cast<std::size_t>(i)<names.size()?names[static_cast<std::size_t>(i)]:std::string_view{};}
template<class T>T enum_value(const Json& value){
 if(value.kind==Json::Kind::number)return static_cast<T>(integer<int>(value));
 if(value.kind==Json::Kind::object){allowed(value,{"name","index"});const auto result=static_cast<T>(integer<int>(required(value,"index")));const auto& name=required(value,"name");const auto expected=enum_name(result);if(expected.empty()){if(name.kind!=Json::Kind::null)invalid("unknown enum requires null name");}else if(native_string(name)!=expected)invalid("enum name differs from index");return result;}
 const auto name=native_string(value);for(std::size_t i=0;i<EnumNames<T>::names.size();++i)if(name==EnumNames<T>::names[i])return static_cast<T>(i);invalid("unknown enum name");
}
template<class T>T read_value(const Json&);
std::string table_json(const EventTable&);
template<class T>void write(std::ostream&,const T&);
template<class T>std::string render(const T& value){auto out=output();write(out,value);return out.str();}
template<class T>void read_into(T& result,const Json& value){
 object(value);const auto members=Record<T>::members();
 for(const auto& [name,item]:value.fields){(void)item;const bool known=std::apply([&](const auto&...entry){return ((name==entry.name)||...);},members);if(!known)invalid("unknown field "+name);}
 std::apply([&](const auto&...entry){([&]{if(const auto* item=field(value,entry.name)){using M=std::remove_cvref_t<decltype(result.*(entry.pointer))>;if constexpr(HasRecord<M>)read_into(result.*(entry.pointer),*item);else result.*(entry.pointer)=read_value<M>(*item);}}(),...);},members);
}
template<class T>T read_record(const Json& value,std::set<std::string>* supplied=nullptr){T result{};read_into(result,value);if(supplied){std::set<std::string> actual;for(const auto& [name,item]:value.fields){(void)item;actual.insert(name);}*supplied=std::move(actual);}return result;}
template<class T,std::size_t I=0>T variant_read(std::size_t index,const Json& value){if constexpr(I==std::variant_size_v<T>)invalid("variant index exceeds native alternatives");else{if(index==I)return T{std::in_place_index<I>,read_value<std::variant_alternative_t<I,T>>(value)};return variant_read<T,I+1>(index,value);}}
std::string integer_text(const Json& value){if(value.kind!=Json::Kind::number||value.raw.empty())invalid("expected integer token");auto digits=value.raw;if(digits.front()=='-')digits.remove_prefix(1);if(digits.empty()||!std::all_of(digits.begin(),digits.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected integer token");return std::string(value.raw);}
template<class T>T read_value(const Json& value){
 if constexpr(std::is_same_v<T,std::nullptr_t>){if(value.kind!=Json::Kind::null)invalid("expected null");return nullptr;}
 else if constexpr(std::is_same_v<T,bool>)return boolean(value);
 else if constexpr(std::is_integral_v<T>)return integer<T>(value);
 else if constexpr(std::is_enum_v<T>)return enum_value<T>(value);
 else if constexpr(std::is_same_v<T,double>){auto items=parse_embedding_workbench_vector_json("["+std::string(value.raw)+"]");if(items.size()!=1)invalid("expected one double");return items.front();}
 else if constexpr(std::is_same_v<T,std::string>)return native_string(value);
 else if constexpr(std::is_same_v<T,TokenInteger>)return TokenInteger(integer_text(value));
 else if constexpr(std::is_same_v<T,CostFunction>){if(value.kind==Json::Kind::null)return {};allowed(value,{"bound"});if(!boolean(required(value,"bound")))invalid("bound callback marker must be true");return {};}
 else if constexpr(Optional<T>::value){if(value.kind==Json::Kind::null)return std::nullopt;return T{read_value<typename Optional<T>::Item>(value)};}
 else if constexpr(Vector<T>::value){if(value.kind!=Json::Kind::array)invalid("expected array");T result;for(const auto& item:value.items)result.push_back(read_value<typename Vector<T>::Item>(item));return result;}
 else if constexpr(Fixed<T>::value){if(value.kind!=Json::Kind::array||value.items.size()!=Fixed<T>::size)invalid("fixed array length mismatch");T result{};for(std::size_t i=0;i<result.size();++i)result[i]=read_value<typename Fixed<T>::Item>(value.items[i]);return result;}
 else if constexpr(Map<T>::value){if(value.kind!=Json::Kind::array)invalid("expected map entry array");T result;for(const auto& item:value.items){allowed(item,{"key","value"});if(!result.emplace(read_value<typename Map<T>::Key>(required(item,"key")),read_value<typename Map<T>::Item>(required(item,"value"))).second)invalid("duplicate map key");}return result;}
 else if constexpr(Variant<T>::value){allowed(value,{"index","value"});return variant_read<T>(integer<std::size_t>(required(value,"index")),required(value,"value"));}
 else if constexpr(std::is_same_v<T,HeuristicStructuredValue>)return {read_value<HeuristicStructuredValue::Value>(value)};
 else if constexpr(std::is_same_v<T,PetriNet>)return parse_oc_playout_workbench_petri_net_json(value.raw);
 else if constexpr(std::is_same_v<T,ProcessTree>){allowed(value,{"activities","root"});return ProcessTree(read_value<std::vector<std::string>>(required(value,"activities")),read_value<ProcessTreeNode>(required(value,"root")));}
 else return read_record<T>(value);
}
void write_attribute(std::ostream&,const EventAttribute&);
void write_native_value(std::ostream& out,const NativeAttributeValue& value){out<<"{\"index\":"<<value.index()<<",\"value\":";std::visit([&](const auto& item){using T=std::decay_t<decltype(item)>;if constexpr(std::is_same_v<T,std::monostate>)out<<"null";else if constexpr(std::is_same_v<T,NativeNA>)out<<"\"NA\"";else if constexpr(std::is_same_v<T,NativeDate>){out<<"{\"value\":"<<q(item.value)<<",\"resolution_index\":"<<static_cast<int>(item.resolution)<<",\"timezone\":";write(out,item.timezone);out<<'}';}else write(out,item);},static_cast<const NativeAttributeValue::Base&>(value));out<<'}';}
void write_attribute(std::ostream& out,const EventAttribute& value){
 out<<"{\"key\":"<<q(value.key)<<",\"value\":"<<q(value.value)<<",\"kind_index\":"<<static_cast<int>(value.kind)<<",\"container_index\":"<<static_cast<int>(value.container)<<",\"null\":"<<(value.is_null?"true":"false")<<",\"scalar_wrapper\":"<<(value.scalar_wrapper?"true":"false")<<",\"scalar_children_list\":"<<(value.scalar_children_list?"true":"false")<<",\"first_class_role_index\":"<<static_cast<int>(value.first_class_role)<<",\"first_class_state_index\":"<<static_cast<int>(value.first_class_state)<<",\"first_class_source_key\":"<<q(value.first_class_source_key)<<",\"typed_value\":";write_native_value(out,value.typed_value);out<<",\"children\":";write(out,value.children);out<<'}';
}
template<class T>void write(std::ostream& out,const T& value){
 if constexpr(std::is_same_v<T,std::nullptr_t>)out<<"null";
 else if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
 else if constexpr(std::is_integral_v<T>)out<<value;
 else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
 else if constexpr(std::is_same_v<T,double>){const auto text=serialize_embedding_workbench_vector_json({value});out.write(text.data()+1,static_cast<std::streamsize>(text.size()-2));}
 else if constexpr(std::is_enum_v<T>){const auto name=enum_name(value);out<<"{\"name\":"<<(name.empty()?"null":q(name))<<",\"index\":"<<static_cast<int>(value)<<'}';}
 else if constexpr(std::is_same_v<T,TokenInteger>)out<<value.decimal();
 else if constexpr(std::is_same_v<T,CostFunction>)out<<(value?"{\"bound\":true}":"null");
 else if constexpr(std::is_same_v<T,EventAttribute>)write_attribute(out,value);
 else if constexpr(std::is_same_v<T,EventLog>)out<<serialize_embedding_workbench_raw_log_json(value);
 else if constexpr(std::is_same_v<T,EventTable>)out<<table_json(value);
 else if constexpr(std::is_same_v<T,EventTableIndexLabel>)write(out,value.value);
 else if constexpr(std::is_same_v<T,PetriNet>)out<<serialize_oc_playout_workbench_petri_net_json(value);
 else if constexpr(std::is_same_v<T,ProcessTree>){out<<"{\"activities\":";write(out,value.activities());out<<",\"root\":";write(out,value.root());out<<'}';}
 else if constexpr(Optional<T>::value){if(value)write(out,*value);else out<<"null";}
 else if constexpr(Vector<T>::value||Fixed<T>::value){out<<'[';bool first=true;for(std::size_t i=0;i<value.size();++i){if(!first)out<<',';first=false;if constexpr(std::is_same_v<T,std::vector<bool>>)write(out,static_cast<bool>(value[i]));else write(out,value[i]);}out<<']';}
 else if constexpr(Map<T>::value){std::map<typename Map<T>::Key,const typename Map<T>::Item*> ordered;for(const auto& [key,item]:value)ordered.emplace(key,&item);out<<'[';bool first=true;for(const auto& [key,item]:ordered){if(!first)out<<',';first=false;out<<"{\"key\":";write(out,key);out<<",\"value\":";write(out,*item);out<<'}';}out<<']';}
 else if constexpr(Variant<T>::value){out<<"{\"index\":"<<value.index()<<",\"value\":";std::visit([&](const auto& item){write(out,item);},value);out<<'}';}
 else if constexpr(std::is_same_v<T,HeuristicStructuredValue>)write(out,value.value);
 else{out<<'{';bool first=true;std::apply([&](const auto&...entry){([&]{if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());out<<'}';}
}

std::string hex(std::string_view value){constexpr char digits[]="0123456789abcdef";std::string out;for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"encoding\":"+q(utf8?"utf8":"hex")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';}
std::string exception_type(const std::exception& error){if(dynamic_cast<const std::invalid_argument*>(&error))return "invalid_argument";if(dynamic_cast<const std::domain_error*>(&error))return "domain_error";if(dynamic_cast<const std::out_of_range*>(&error))return "out_of_range";if(dynamic_cast<const std::overflow_error*>(&error))return "overflow_error";if(dynamic_cast<const std::length_error*>(&error))return "length_error";if(dynamic_cast<const std::runtime_error*>(&error))return "runtime_error";return "exception";}
std::string error_json(std::string_view phase,const std::exception& error){return "{\"phase\":"+q(phase)+",\"exception_type\":"+q(exception_type(error))+",\"message\":"+q(error.what())+'}';}
std::string typed(std::string_view type,std::string_view bytes){return "{\"type\":"+q(type)+",\"value\":"+std::string(bytes)+'}';}

// Complete raw stream carrier: hexadecimal native strings share the existing
// raw-log attribute/metadata grammar, without a legacy inspection projection.
void hex_attribute(EventAttribute& value){value.key=hex(value.key);value.value=hex(value.value);value.first_class_source_key=hex(value.first_class_source_key);if(auto* date=std::get_if<NativeDate>(&value.typed_value)){date->value=hex(date->value);if(date->timezone)*date->timezone=hex(*date->timezone);}for(auto& child:value.children)hex_attribute(child);}
void hex_attributes(std::vector<EventAttribute>& values){for(auto& value:values)hex_attribute(value);}
void hex_globals(std::vector<LogGlobal>& values){for(auto& value:values){value.scope=hex(value.scope);hex_attributes(value.attributes);}}
void hex_metadata(EventDataMetadata& value){for(auto& e:value.extensions){e.name=hex(e.name);e.prefix=hex(e.prefix);e.uri=hex(e.uri);}for(auto& c:value.classifiers){c.name=hex(c.name);for(auto& key:c.keys)key=hex(key);}hex_attributes(value.trace_globals);hex_attributes(value.event_globals);hex_attributes(value.properties);hex_globals(value.globals);hex_globals(value.compatibility_globals_snapshot);hex_attributes(value.compatibility_trace_globals_snapshot);hex_attributes(value.compatibility_event_globals_snapshot);}

// The shared embedding table writer's valid scalar path is an inspection
// format: it drops the four stored compatibility metadata fields. This scoped
// writer always uses the existing full raw constructor grammar instead.
void hex_index_label(EventTableIndexLabel& value){if(auto* attr=std::get_if<EventAttribute>(&value.value))hex_attribute(*attr);else for(auto& child:std::get<std::vector<EventTableIndexLabel>>(value.value))hex_index_label(child);}
std::string table_json(const EventTable& value){
 auto shadow=value;for(auto& column:shadow.columns)column=hex(column);for(auto& row:shadow.rows)for(auto& cell:row)if(cell)hex_attribute(*cell);hex_attributes(shadow.attributes);hex_metadata(shadow.metadata);for(auto& dtype:shadow.dtypes)dtype=hex(dtype);
 if(shadow.index){auto& index=*shadow.index;index.kind=hex(index.kind);index.dtype=hex(index.dtype);for(auto& name:index.names)if(name)*name=hex(*name);for(auto& label:index.labels)hex_index_label(label);}
 for(auto& dtype:shadow.categorical_dtypes)if(dtype){dtype->categories_dtype=hex(dtype->categories_dtype);hex_attributes(dtype->categories);}
 auto out=output();out<<"{\"native_byte_strings\":\"hex-v1\",\"native_schema\":\"event-table-raw-v1\",\"value\":{\"columns\":";write(out,shadow.columns);out<<",\"rows\":";write(out,shadow.rows);out<<",\"attributes\":";write(out,shadow.attributes);out<<",\"metadata\":";write(out,shadow.metadata);out<<",\"dtypes\":";write(out,shadow.dtypes);out<<",\"index\":";write(out,shadow.index);out<<",\"categorical_dtypes\":";write(out,shadow.categorical_dtypes);out<<"}}";return out.str();
}

std::string stream_json(const EventStream& value){auto shadow=value;hex_attributes(shadow.attributes);hex_metadata(shadow.metadata);for(auto& event:shadow.events)hex_attributes(event.fields);return "{\"native_byte_strings\":\"hex-v1\",\"native_schema\":\"event-stream-raw-v1\",\"value\":"+render(shadow)+'}';}
EventStream stream_value(const Json& value){
 const Json* content=&value;bool wrapped=false;
 if(field(value,"native_byte_strings")){allowed(value,{"native_byte_strings","native_schema","value"});if(native_string(required(value,"native_byte_strings"))!="hex-v1"||native_string(required(value,"native_schema"))!="event-stream-raw-v1")invalid("unknown raw stream wrapper");content=&required(value,"value");wrapped=true;}
 allowed(*content,{"attributes","metadata","events"});
 const auto parse_log=[&](std::string body){if(wrapped)body="{\"native_byte_strings\":\"hex-v1\",\"value\":"+body+'}';return parse_embedding_workbench_raw_log_json(body);};
 EventStream result;std::string holder="{";if(const auto* attrs=field(*content,"attributes"))holder+="\"attributes\":"+std::string(attrs->raw);if(const auto* metadata=field(*content,"metadata")){if(holder.size()>1)holder+=',';holder+="\"metadata\":"+std::string(metadata->raw);}holder+='}';auto parsed=parse_log(holder);result.attributes=std::move(parsed.attributes);result.metadata=std::move(parsed.metadata);
 if(const auto* events=field(*content,"events")){if(events->kind!=Json::Kind::array)invalid("expected events array");for(const auto& event:events->items){allowed(event,{"fields"});EventRecord record;if(const auto* attrs=field(event,"fields"))record.fields=parse_log("{\"attributes\":"+std::string(attrs->raw)+'}').attributes;result.events.push_back(std::move(record));}}
 return result;
}

struct Outcome{bool throws=false;double value=0.0;std::string message;};
struct CallbackEntry{std::string label;std::size_t index=0;Outcome outcome;};
struct CallbackTable{std::vector<CallbackEntry> entries;Outcome fallback;};
struct CallbackInvocation{std::string slot,label;std::size_t index=0;std::optional<std::size_t> entry_index;bool used_fallback=false;Outcome outcome;};
using Tables=std::array<std::optional<CallbackTable>,3>;
constexpr std::array<std::string_view,3> slots={"trace_cost_function","model_cost_function","sync_cost_function"};
Outcome outcome_value(const Json& value){allowed(value,{"value","throw"});if(bool(field(value,"value"))==bool(field(value,"throw")))invalid("callback outcome requires one value or throw");Outcome out;if(const auto* item=field(value,"throw")){out.throws=true;out.message=native_string(*item);}else out.value=read_value<double>(required(value,"value"));return out;}
std::string outcome_json(const Outcome& value){return value.throws?"{\"throw\":"+q(value.message)+'}':"{\"value\":"+render(value.value)+'}';}
CallbackTable table_value(const Json& value){allowed(value,{"entries","fallback"});CallbackTable out;if(const auto* entries=field(value,"entries")){if(entries->kind!=Json::Kind::array)invalid("callback entries must be array");for(const auto& item:entries->items){allowed(item,{"label","index","outcome"});out.entries.push_back({native_string(required(item,"label")),integer<std::size_t>(required(item,"index")),outcome_value(required(item,"outcome"))});}}if(const auto* fallback=field(value,"fallback"))out.fallback=outcome_value(*fallback);return out;}
Tables tables_value(const Json& value){allowed(value,{slots[0],slots[1],slots[2]});Tables out;for(std::size_t i=0;i<out.size();++i)if(const auto* item=field(value,slots[i]);item&&item->kind!=Json::Kind::null)out[i]=table_value(*item);return out;}
std::string tables_json(const Tables& tables){std::string out="{";for(std::size_t i=0;i<tables.size();++i){if(i)out+=',';out+=q(slots[i])+':';if(!tables[i]){out+="null";continue;}out+="{\"entries\":[";bool first=true;for(const auto& entry:tables[i]->entries){if(!first)out+=',';first=false;out+="{\"label\":"+q(entry.label)+",\"index\":"+std::to_string(entry.index)+",\"outcome\":"+outcome_json(entry.outcome)+'}';}out+="],\"fallback\":"+outcome_json(tables[i]->fallback)+'}';}return out+'}';}
std::string invocations_json(const std::vector<CallbackInvocation>& values){std::string out="[";for(const auto& v:values){if(out.size()>1)out+=',';out+="{\"slot\":"+q(v.slot)+",\"label\":"+q(v.label)+",\"index\":"+std::to_string(v.index)+",\"entry_index\":"+(v.entry_index?std::to_string(*v.entry_index):"null")+",\"used_fallback\":"+(v.used_fallback?"true":"false")+",\"outcome\":"+outcome_json(v.outcome)+'}';}return out+']';}
void bind_callbacks(ReplayFitnessOptions& options,const Tables& tables,const Json* option_document,const std::shared_ptr<std::vector<CallbackInvocation>>& invocations){
 const Json* alignment=option_document?field(*option_document,"alignment_options"):nullptr;
 std::array<CostFunction*,3> functions={&options.alignment_options.trace_cost_function,&options.alignment_options.model_cost_function,&options.alignment_options.sync_cost_function};
 for(std::size_t i=0;i<tables.size();++i)if(alignment)if(const auto* explicit_marker=field(*alignment,slots[i]);explicit_marker&&(explicit_marker->kind!=Json::Kind::null)!=bool(tables[i]))invalid("callback marker disagrees with actual binding");
 for(std::size_t i=0;i<tables.size();++i){if(!tables[i]){*functions[i]={};continue;}const auto table=*tables[i];const auto slot=std::string(slots[i]);*functions[i]=[table,slot,invocations](const std::string& label,std::size_t index){std::optional<std::size_t> selected;for(std::size_t j=0;j<table.entries.size();++j)if(table.entries[j].label==label&&table.entries[j].index==index){selected=j;break;}const auto outcome=selected?table.entries[*selected].outcome:table.fallback;invocations->push_back({slot,label,index,selected,!selected,outcome});if(outcome.throws)throw std::runtime_error(outcome.message);return outcome.value;};}
}
std::string callback_presence(const ReplayFitnessOptions* options){const std::array<bool,3> flags={options&&bool(options->alignment_options.trace_cost_function),options&&bool(options->alignment_options.model_cost_function),options&&bool(options->alignment_options.sync_cost_function)};std::string out="{";for(std::size_t i=0;i<flags.size();++i){if(i)out+=',';out+=q(slots[i])+':'+(flags[i]?"true":"false");}return out+'}';}

struct LazyScan{std::size_t scan_index=0;std::vector<std::size_t> rows;bool returned=false;std::string error="null";};
std::string scans_json(const std::vector<LazyScan>& values){std::string out="[";for(const auto& v:values){if(out.size()>1)out+=',';out+="{\"scan_index\":"+std::to_string(v.scan_index)+",\"rows\":"+render(v.rows)+",\"returned\":"+(v.returned?"true":"false")+",\"error\":"+v.error+'}';}return out+']';}
struct Source{
 std::string kind;EventLog log;EventStream stream;EventTable table;LazyEventTable lazy;bool valid=false;std::optional<std::size_t> row_count;
 std::shared_ptr<std::vector<LazyScan>> scans=std::make_shared<std::vector<LazyScan>>();
 EventDataView view()const{if(kind=="log")return EventDataView(log);if(kind=="stream")return EventDataView(stream);if(kind=="table")return EventDataView(table);return EventDataView(lazy);}
 std::string snapshot()const{if(kind=="log")return serialize_embedding_workbench_raw_log_json(log);if(kind=="stream")return stream_json(stream);if(kind=="table")return table_json(table);return "{\"table\":"+table_json(table)+",\"valid\":"+(valid?"true":"false")+",\"row_count\":"+(row_count?std::to_string(*row_count):"null")+'}';}
 void prepare(std::string_view requested,std::string_view bytes){
  kind=std::string(requested.starts_with("view_")?requested.substr(5):requested);
  if(kind=="log")log=parse_embedding_workbench_raw_log_json(bytes);
  else if(kind=="stream")stream=stream_value(Parser(bytes).parse());
  else if(kind=="table")table=parse_embedding_workbench_table_json(bytes);
  else{const auto doc=Parser(bytes).parse();allowed(doc,{"table","valid","row_count"});table=parse_embedding_workbench_table_json(required(doc,"table").raw);valid=field(doc,"valid")?boolean(required(doc,"valid")):true;row_count=valid?std::make_optional(table.rows.size()):std::nullopt;if(const auto* count=field(doc,"row_count"))row_count=count->kind==Json::Kind::null?std::nullopt:std::make_optional(integer<std::size_t>(*count));
   auto static_state=table;static_state.rows.clear();if(!valid&&!row_count&&table_json(static_state)==table_json(EventTable{})){lazy=LazyEventTable{};return;}
   LazyEventTable::Scanner scanner;auto owned=std::make_shared<const EventTable>(table);const auto history=scans;
   if(valid)scanner=[owned,history](const LazyEventTable::RowConsumer& consumer){const auto index=history->size();history->push_back({index,{},false,"null"});try{for(std::size_t row=0;row<owned->rows.size();++row){history->at(index).rows.push_back(row);consumer(owned->rows[row]);}history->at(index).returned=true;}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){history->at(index).error=error_json("native_error",error);throw;}};
   lazy=LazyEventTable(table.columns,std::move(scanner),table.attributes,table.metadata,row_count,table.dtypes,table.index,table.categorical_dtypes);
  }
 }
};
using Model=std::variant<PetriNet,ProcessTree,PnmlDocument,PetriNetPresentation>;
using Options=std::variant<std::monostate,ReplayFitnessOptions,PrecisionOptions,GeneralizationOptions,SimplicityOptions,CompositeEvaluationOptions>;
using Result=std::variant<std::monostate,ReplayFitnessResult,PrecisionResult,GeneralizationResult,SimplicityResult,CompositeEvaluationResult>;
std::string options_json(const Options& options){return std::visit([](const auto& value)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(value)>,std::monostate>)return "null";else return render(value);},options);}
std::string model_json(const Model& model){return std::visit([](const auto& value){return render(value);},model);}
constexpr std::array<std::string_view,5> operations={"evaluate_replay_fitness","evaluate_precision","evaluate_generalization","evaluate_simplicity","evaluate_composite"};
constexpr std::array<std::string_view,5> option_types={"ReplayFitnessOptions","PrecisionOptions","GeneralizationOptions","SimplicityOptions","CompositeEvaluationOptions"};
constexpr std::array<std::string_view,5> result_types={"ReplayFitnessResult","PrecisionResult","GeneralizationResult","SimplicityResult","CompositeEvaluationResult"};
constexpr std::array<std::string_view,4> variant_types={"ReplayFitnessVariant","PrecisionVariant","GeneralizationVariant","SimplicityVariant"};
std::string source_type(std::string_view kind){if(kind.starts_with("view_"))return "EventDataView";if(kind=="log")return "EventLog";if(kind=="stream")return "EventStream";if(kind=="table")return "EventTable";return "LazyEventTable";}
std::string model_type(std::string_view kind){if(kind=="petri_net")return "PetriNet";if(kind=="process_tree")return "ProcessTree";if(kind=="pnml_document")return "PnmlDocument";return "PetriNetPresentation";}

// The template branches compile only the actual declared overload combinations.
// Direct wrappers remain direct calls; view arms keep their original owner.
template<class S,class M>Result invoke_source(int family,const S& source,const M& model,int selected_variant,const Options& options,bool defaults){
 if constexpr(std::is_same_v<M,PetriNetPresentation>){invalid("presentation is simplicity-only");}
 else{
  if(family==4)return evaluate_composite(source,model,std::get<CompositeEvaluationOptions>(options));
  if constexpr(std::is_same_v<M,PnmlDocument>&&(!std::is_same_v<S,EventLog>&&!std::is_same_v<S,EventDataView>)){invalid("no direct flat PNML overload in this family");}
  else{
   if(family==0){const auto variant=static_cast<ReplayFitnessVariant>(selected_variant);if constexpr(std::is_same_v<S,EventLog>&&!std::is_same_v<M,PnmlDocument>)if(defaults)return evaluate_replay_fitness(source,model,variant);return evaluate_replay_fitness(source,model,variant,std::get<ReplayFitnessOptions>(options));}
   if(family==1){const auto variant=static_cast<PrecisionVariant>(selected_variant);if constexpr(std::is_same_v<S,EventLog>&&!std::is_same_v<M,PnmlDocument>)if(defaults)return evaluate_precision(source,model,variant);return evaluate_precision(source,model,variant,std::get<PrecisionOptions>(options));}
   if(family==2){const auto variant=static_cast<GeneralizationVariant>(selected_variant);if constexpr(std::is_same_v<S,EventLog>&&!std::is_same_v<M,PnmlDocument>)if(defaults)return evaluate_generalization(source,model,variant);return evaluate_generalization(source,model,variant,std::get<GeneralizationOptions>(options));}
  }
 }
 invalid("unknown source evaluation family");
}
Result invoke(int family,std::string_view source_kind,const Source& source,const Model& model,int variant,const Options& options,bool defaults){return std::visit([&](const auto& concrete)->Result{if(family==3)return evaluate_simplicity(concrete,static_cast<SimplicityVariant>(variant),std::get<SimplicityOptions>(options));if(source_kind.starts_with("view_"))return invoke_source(family,source.view(),concrete,variant,options,defaults);if(source.kind=="log")return invoke_source(family,source.log,concrete,variant,options,defaults);if(source.kind=="stream")return invoke_source(family,source.stream,concrete,variant,options,defaults);if(source.kind=="table")return invoke_source(family,source.table,concrete,variant,options,defaults);return invoke_source(family,source.lazy,concrete,variant,options,defaults);},model);}

bool input_role(std::string_view name){return name=="source_file"||name=="model_file";}
std::string snapshots(const std::array<std::string,2>& values){return "{\"source\":"+values[0]+",\"model\":"+values[1]+'}';}
QualityWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& part,bool query_part){by_name[part.name].push_back(&part);if(!query_part){if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(part.name)+",\"filename\":"+(part.file?q(part.filename):"null")+",\"file\":"+(part.file?"true":"false")+",\"original\":"+carrier(part.file?part.filename:part.name,part.bytes)+'}';}if(query_part||!input_role(part.name)){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(part.name)+",\"origin\":"+q(query_part?"query":"multipart")+'}';}};
 for(const auto& p:query)add(p,true);for(const auto& p:parts)add(p,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto it=by_name.find(std::string(name));return it==by_name.end()||it->second.size()!=1?nullptr:it->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* p=find(name);return p?p->bytes:std::string(fallback);};
 const auto operation=control("operation",operations[0]);const auto source_kind=control("source_kind","log");const auto model_kind=control("model_kind","petri_net");const auto call_form=control("call_form","with_options");
 int family=-1,variant=0;Options options;Result result;Source source;Model model;bool selected=false,source_prepared=false,model_prepared=false,entered=false,returned=false,callbacks_prepared=false;Tables tables;
 auto callbacks=std::make_shared<std::vector<CallbackInvocation>>();std::array<std::string,2> before={"null","null"},after=before;std::optional<bool> source_unchanged,model_unchanged,options_unchanged;
 std::vector<std::string> parsed;std::set<std::string> supplied;std::string phase="configuration_error",failure="null",calls="[",configured="null",configured_tables="null",result_holder="null";std::optional<Json> option_document;
 std::map<std::string,std::optional<std::string>> artifacts;for(const auto* key:{"source_input_json","model_input_json","source_after_json","model_after_json","configured_options_json","configured_callback_tables_json","callback_invocations_json","full_result_json"})artifacts.emplace(key,std::nullopt);
 const auto call=[&](std::string_view name,std::string_view stage,std::string_view kind,const auto& function){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind);try{function();calls+=prefix+",\"returned\":true,\"error\":null}";}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){calls+=prefix+",\"returned\":false,\"error\":"+error_json(stage=="preparation"?"preparation_error":"native_error",error)+'}';throw;}};
 const auto snapshot_after=[&]{if(!entered)return;if(source_prepared){after[0]=source.snapshot();artifacts["source_after_json"]=after[0];source_unchanged=after[0]==before[0];}if(model_prepared){after[1]=model_json(model);artifacts["model_after_json"]=after[1];model_unchanged=after[1]==before[1];}if(options.index())options_unchanged=configured==options_json(options);};
 try{
  if(transport_error)invalid(*transport_error);
  for(const auto& [name,values]:by_name){if(values.size()!=1)invalid("duplicate control or upload: "+name);if(name!="operation"&&name!="source_kind"&&name!="model_kind"&&name!="call_form"&&name!="variant"&&name!="options_json"&&name!="callback_tables_json"&&!input_role(name))invalid("unknown control or upload: "+name);}
  for(const auto& p:query)if(input_role(p.name))invalid("input upload cannot be a query control");
  for(std::size_t i=0;i<operations.size();++i)if(operation==operations[i])family=static_cast<int>(i);if(family<0)invalid("unknown public operation");
  const bool simplicity=family==3,defaults=call_form=="default_options",document=model_kind=="pnml_document";
  if(model_kind!="petri_net"&&model_kind!="process_tree"&&!document&&model_kind!="presentation")invalid("unknown model kind");
  if(model_kind=="presentation"&&!simplicity)invalid("presentation is simplicity-only");
  if(!simplicity&&source_kind!="log"&&source_kind!="stream"&&source_kind!="table"&&source_kind!="lazy"&&source_kind!="view_log"&&source_kind!="view_stream"&&source_kind!="view_table"&&source_kind!="view_lazy")invalid("unknown source kind");
  if(call_form!="with_options"&&!defaults)invalid("unknown call form");
  if(defaults&&(family>2||source_kind!="log"||document))invalid("no default-options overload for selected types");
  if(!simplicity&&document&&family!=4&&(source_kind=="stream"||source_kind=="table"||source_kind=="lazy"))invalid("no direct flat PNML overload for selected family");
  if(simplicity&&(find("source_file")||find("source_kind")))invalid("source is inapplicable to simplicity");
  if(family==4&&find("variant"))invalid("variant is inapplicable to composite");
  if(defaults&&(find("options_json")||find("callback_tables_json")))invalid("options are inapplicable to default-options overload");
  if((family!=0||defaults)&&find("callback_tables_json"))invalid("callback tables are replay with-options only");
  selected=true;parsed.push_back("operation");if(!simplicity)parsed.push_back("source_kind");parsed.push_back("model_kind");parsed.push_back("call_form");
  if(family!=4){const auto* submitted=find("variant");if(family==1&&document&&!submitted)invalid("precision PNML overload requires explicit variant");if(submitted){const auto& bytes=submitted->bytes;const bool json=!bytes.empty()&&(bytes.front()=='{'||bytes.front()=='"'||bytes.front()=='-'||(bytes.front()>='0'&&bytes.front()<='9'));const auto raw=json?bytes:q(bytes);const auto value=Parser(raw).parse();if(family==0)variant=static_cast<int>(enum_value<ReplayFitnessVariant>(value));else if(family==1)variant=static_cast<int>(enum_value<PrecisionVariant>(value));else if(family==2)variant=static_cast<int>(enum_value<GeneralizationVariant>(value));else variant=static_cast<int>(enum_value<SimplicityVariant>(value));parsed.push_back("variant");}else variant=family==0&&document?static_cast<int>(ReplayFitnessVariant::TOKEN_BASED):0;}
  if(!defaults){const auto* p=find("options_json");const auto bytes=p?p->bytes:std::string("{}");option_document=Parser(bytes).parse();if(family==0)options=read_record<ReplayFitnessOptions>(*option_document,&supplied);else if(family==1)options=read_record<PrecisionOptions>(*option_document,&supplied);else if(family==2)options=read_record<GeneralizationOptions>(*option_document,&supplied);else if(family==3)options=read_record<SimplicityOptions>(*option_document,&supplied);else options=read_record<CompositeEvaluationOptions>(*option_document,&supplied);if(p)parsed.push_back("options_json");
   if(family==0){try{if(const auto* p_tables=find("callback_tables_json")){tables=tables_value(Parser(p_tables->bytes).parse());}bind_callbacks(std::get<ReplayFitnessOptions>(options),tables,&*option_document,callbacks);}catch(const std::bad_alloc&){throw;}catch(const std::exception&){const auto original=std::current_exception();phase="result_serialization_error";configured=options_json(options);artifacts["configured_options_json"]=configured;phase="configuration_error";std::rethrow_exception(original);}phase="result_serialization_error";configured_tables=tables_json(tables);artifacts["configured_callback_tables_json"]=configured_tables;phase="configuration_error";callbacks_prepared=true;if(find("callback_tables_json"))parsed.push_back("callback_tables_json");}
   phase="result_serialization_error";configured=options_json(options);artifacts["configured_options_json"]=configured;phase="configuration_error";
  }
  phase="preparation_error";
  if(!simplicity){const auto* p=find("source_file");if(!p)invalid("source_file is required");call("prepare_original_"+source_kind,"preparation",source_type(source_kind),[&]{source.prepare(source_kind,p->bytes);});phase="result_serialization_error";before[0]=source.snapshot();artifacts["source_input_json"]=before[0];source_prepared=true;phase="preparation_error";}
  const auto* p_model=find("model_file");if(!p_model)invalid("model_file is required");call("prepare_original_"+model_kind,"preparation",model_type(model_kind),[&]{if(model_kind=="petri_net")model=parse_oc_playout_workbench_petri_net_json(p_model->bytes);else if(model_kind=="process_tree")model=read_value<ProcessTree>(Parser(p_model->bytes).parse());else if(document)model=read_record<PnmlDocument>(Parser(p_model->bytes).parse());else model=read_record<PetriNetPresentation>(Parser(p_model->bytes).parse());});
  phase="result_serialization_error";before[1]=model_json(model);artifacts["model_input_json"]=before[1];model_prepared=true;
  std::string signature=operation+'(';if(!simplicity)signature+="const "+source_type(source_kind)+"&,";signature+="const "+model_type(model_kind)+'&';if(family!=4)signature+=','+std::string(variant_types[static_cast<std::size_t>(family)]);if(!defaults)signature+=",const "+std::string(option_types[static_cast<std::size_t>(family)])+'&';signature+=')';
  phase="native_error";
  try{call(signature,"primary",simplicity?model_type(model_kind):source_type(source_kind)+'+'+model_type(model_kind),[&]{entered=true;result=invoke(family,source_kind,source,model,variant,options,defaults);returned=true;});}catch(const std::bad_alloc&){throw;}catch(const std::exception&){const auto original=std::current_exception();phase="result_serialization_error";snapshot_after();phase="native_error";std::rethrow_exception(original);}
  phase="result_serialization_error";snapshot_after();const auto body=std::visit([](const auto& value)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(value)>,std::monostate>)return "null";else return render(value);},result);result_holder=typed(result_types[static_cast<std::size_t>(family)],body);artifacts["full_result_json"]=result_holder;
 }catch(const std::bad_alloc&){throw;}catch(const std::exception& error){failure=error_json(phase,error);}
 calls+=']';const auto invocation_text=invocations_json(*callbacks);if(callbacks_prepared)artifacts["callback_invocations_json"]=invocation_text;
 std::string artifact_map="{";for(const auto& [key,bytes]:artifacts){if(artifact_map.size()>1)artifact_map+=',';artifact_map+=q(key)+':'+(bytes?carrier(key+".json",*bytes):"null");}artifact_map+='}';
 const auto maybe=[](const std::optional<bool>& value){return value?(*value?"true":"false"):"null";};const auto* submitted=find("options_json");const auto* replay=std::get_if<ReplayFitnessOptions>(&options);
 QualityWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_QUALITY_WORKBENCH\",\"version\":\"original-quality-workbench-v1\",\"operation\":"+q(operation)+",\"source_kind\":"+((family==3&&!find("source_kind"))?"null":q(source_kind))+",\"model_kind\":"+q(model_kind)+",\"call_form\":"+q(call_form)+",\"executed\":"+(returned?"true":"false")+",\"native_entered\":"+(entered?"true":"false")+",\"native_returned\":"+(returned?"true":"false")+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls
 +",\"input_context\":{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_type\":"+(selected&&family!=3?q(source_type(source_kind)):"null")+",\"model_type\":"+(selected?q(model_type(model_kind)):"null")+",\"source_prepared\":"+(source_prepared?"true":"false")+",\"model_prepared\":"+(model_prepared?"true":"false")+",\"source_unchanged\":"+maybe(source_unchanged)+",\"model_unchanged\":"+maybe(model_unchanged)+",\"options_unchanged\":"+maybe(options_unchanged)+",\"callback_binding_presence\":"+callback_presence(replay)+",\"lazy_scans\":"+scans_json(*source.scans)+'}'
 +",\"input\":{\"before\":"+snapshots(before)+",\"after\":"+snapshots(after)+"},\"submitted_options\":"+(submitted?carrier(submitted->file?submitted->filename:submitted->name,submitted->bytes):"null")+",\"configured_options\":"+configured+",\"supplied_options\":"+render(std::vector<std::string>(supplied.begin(),supplied.end()))+",\"configured_callback_tables\":"+configured_tables+",\"callback_invocations\":"+invocation_text+",\"result\":"+result_holder+",\"artifact_bytes\":"+artifact_map+",\"artifact_errors\":[]}";return response;
}
} // namespace
ReplayFitnessOptions parse_quality_replay_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<ReplayFitnessOptions>(Parser(value).parse(),supplied);}
std::string serialize_quality_workbench_options_json(const ReplayFitnessOptions& value){return render(value);}
PrecisionOptions parse_quality_precision_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<PrecisionOptions>(Parser(value).parse(),supplied);}
std::string serialize_quality_workbench_options_json(const PrecisionOptions& value){return render(value);}
GeneralizationOptions parse_quality_generalization_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<GeneralizationOptions>(Parser(value).parse(),supplied);}
std::string serialize_quality_workbench_options_json(const GeneralizationOptions& value){return render(value);}
SimplicityOptions parse_quality_simplicity_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<SimplicityOptions>(Parser(value).parse(),supplied);}
std::string serialize_quality_workbench_options_json(const SimplicityOptions& value){return render(value);}
CompositeEvaluationOptions parse_quality_composite_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<CompositeEvaluationOptions>(Parser(value).parse(),supplied);}
std::string serialize_quality_workbench_options_json(const CompositeEvaluationOptions& value){return render(value);}
std::string serialize_quality_workbench_result_json(const ReplayFitnessResult& value){return render(value);}
std::string serialize_quality_workbench_result_json(const PrecisionResult& value){return render(value);}
std::string serialize_quality_workbench_result_json(const GeneralizationResult& value){return render(value);}
std::string serialize_quality_workbench_result_json(const SimplicityResult& value){return render(value);}
std::string serialize_quality_workbench_result_json(const CompositeEvaluationResult& value){return render(value);}
ProcessTree parse_quality_workbench_tree_json(std::string_view value){return read_value<ProcessTree>(Parser(value).parse());}
std::string serialize_quality_workbench_tree_json(const ProcessTree& value){return render(value);}
PnmlDocument parse_quality_workbench_document_json(std::string_view value){return read_value<PnmlDocument>(Parser(value).parse());}
std::string serialize_quality_workbench_document_json(const PnmlDocument& value){return render(value);}
PetriNetPresentation parse_quality_workbench_presentation_json(std::string_view value){return read_value<PetriNetPresentation>(Parser(value).parse());}
std::string serialize_quality_workbench_presentation_json(const PetriNetPresentation& value){return render(value);}
EventStream parse_quality_workbench_stream_json(std::string_view value){return stream_value(Parser(value).parse());}
std::string serialize_quality_workbench_stream_json(const EventStream& value){return stream_json(value);}
std::string serialize_quality_workbench_table_json(const EventTable& value){return table_json(value);}
QualityWorkbenchResponse execute_quality_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query){return execute(parts,query);}
QualityWorkbenchResponse execute_quality_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){if(!complete)return execute({},query,"multipart transport did not complete");std::vector<DiscoveryWorkbenchPart> parts;try{parts=parse_discovery_workbench_multipart(content_type,body);}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){return execute({},query,error.what());}return execute(parts,query);}
} // namespace minera
