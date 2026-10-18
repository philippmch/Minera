#include "minera/petri_analysis_workbench_json.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <locale>
#include <map>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/petri_analysis_json.hpp"

namespace minera {
namespace {
[[noreturn]] void invalid(std::string_view message) {
  throw std::invalid_argument("petri analysis workbench JSON: " + std::string(message));
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
template<class T> struct Vector : std::false_type {};
template<class T> struct Vector<std::vector<T>> : std::true_type { using Item = T; };
template<class T> struct Optional : std::false_type {};
template<class T> struct Optional<std::optional<T>> : std::true_type {};
template<class T, class M> struct Member { std::string_view name; M T::* pointer; };
template<class T, class M> auto member(std::string_view name, M T::* pointer) { return Member<T, M>{name, pointer}; }
template<class T> struct Record;
#define F(name) member(#name, &V::name)
#define RECORD(Type, ...) template<> struct Record<Type> { static auto members() { using V = Type; return std::make_tuple(__VA_ARGS__); } };
RECORD(MarkingEquationOptions, F(initial_marking), F(final_marking), F(transition_costs), F(realizability_limit))
RECORD(ExtendedMarkingEquationOptions, F(initial_marking), F(final_marking), F(model_transition_costs), F(synchronous_transition_costs), F(log_move_costs), F(split_indices), F(max_k_value), F(full_bootstrap_required), F(realizability_limit))
RECORD(SoundnessOptions, F(variant), F(initial_marking), F(final_marking), F(max_states), F(invariant_search_limit), F(diagnostics), F(return_asap_when_unsound))
RECORD(WorkflowMarkings, F(initial), F(final))
RECORD(MarkingEquationResult, F(variant), F(computed), F(feasible), F(exhausted), F(solver_nodes), F(objective), F(lower_bound), F(transition_counts), F(residual), F(activated_transitions), F(firing_sequence), F(recovery_attempted), F(reaches_final), F(error))
RECORD(ExtendedMarkingEquationResult, F(variant), F(computed), F(feasible), F(exhausted), F(solver_nodes), F(objective), F(lower_bound), F(retained_splits), F(segment_count), F(transition_counts), F(firing_sequence), F(reaches_final), F(error))
RECORD(WorkflowNetResult, F(variant), F(valid), F(source_candidates), F(sink_candidates), F(offending_places), F(offending_transitions), F(unreachable_places), F(unreachable_transitions), F(reason))
RECORD(SoundnessResult, F(variant), F(status), F(computed), F(exhausted), F(returned_asap), F(workflow), F(bounded), F(final_reachable), F(option_to_complete), F(proper_completion), F(dead_transitions), F(dead_tasks), F(deadlocks), F(deadlock_witnesses), F(improper_markings), F(improper_witnesses), F(completion_witness), F(locking_witness), F(locking_scenarios), F(unbounded_witness), F(reachable_marking_count), F(coverability_node_count), F(diagnostic_messages), F(place_invariant_basis), F(uncovered_places), F(uniform_place_invariants), F(uncovered_places_uniform), F(uniform_invariant_search_exhausted), F(weighted_place_invariants), F(uncovered_places_weighted), F(uncovered_places_s_component), F(left), F(coverability_markings), F(coverability_edges), F(restricted_coverability_tree_exhausted), F(restricted_coverability_markings), F(restricted_coverability_edges), F(reachability_markings), F(reachability_edges), F(s_components), F(handled_pairs), F(not_well_handled_pairs), F(structural_node_labels), F(short_circuit_valid), F(short_circuit_strongly_connected), F(short_circuit_transition), F(short_circuit_diagnostic_messages), F(short_circuit_reachability_exhausted), F(short_circuit_reachability_strongly_connected), F(short_circuit_reachability_markings), F(short_circuit_reachability_edges), F(error))
#undef RECORD
#undef F

template<class T> std::string_view enum_name(T value) {
  const auto index = static_cast<std::int32_t>(value);
  if constexpr (std::is_same_v<T, SoundnessVariant>) return index == 0 ? "soundness" : index == 1 ? "woflan" : "";
  else if constexpr (std::is_same_v<T, SoundnessStatus>) return index == 0 ? "sound" : index == 1 ? "unsound" : index == 2 ? "unknown" : "";
  else if constexpr (std::is_same_v<T, WorkflowNetVariant>) return index == 0 ? "petri_net" : "";
  else return index == 0 ? "classic" : "";
}
SoundnessVariant variant(const Json& value) {
  if (value.kind == Json::Kind::string) {
    const auto name = string_token(value.raw);
    if (name == "soundness") return SoundnessVariant::kSoundness;
    if (name == "woflan") return SoundnessVariant::kWoflan;
    invalid("unknown soundness variant name");
  }
  if (value.kind != Json::Kind::object || value.fields.size() != 2 || !field(value, "name") || !field(value, "index")) invalid("variant requires an exact name/index object");
  const auto result = static_cast<SoundnessVariant>(integer<std::int32_t>(*field(value, "index")));
  const auto expected = enum_name(result); const auto& name = *field(value, "name");
  if (expected.empty()) { if (name.kind != Json::Kind::null) invalid("unknown variant index requires null name"); }
  else if (name.kind != Json::Kind::string || string_token(name.raw) != expected) invalid("variant name/index mismatch");
  return result;
}
template<class T> T read_value(const Json& value) {
  if constexpr (std::is_same_v<T, bool>) { if (value.kind != Json::Kind::boolean) invalid("expected boolean"); return value.raw == "true"; }
  else if constexpr (std::is_integral_v<T>) return integer<T>(value);
  else if constexpr (std::is_same_v<T, SoundnessVariant>) return variant(value);
  else if constexpr (std::is_same_v<T, std::vector<double>>) return parse_embedding_workbench_vector_json(value.raw);
  else if constexpr (Vector<T>::value) {
    if (value.kind != Json::Kind::array) invalid("expected array");
    T result; result.reserve(value.items.size()); for (const auto& item : value.items) result.push_back(read_value<typename Vector<T>::Item>(item)); return result;
  }
}
template<class T> T options(std::string_view text, std::set<std::string>* supplied) {
  const auto root = Parser(text).parse(); if (root.kind != Json::Kind::object) invalid("options must be an object");
  std::set<std::string> allowed;
  std::apply([&](const auto&... entry) { (allowed.insert(std::string(entry.name)), ...); }, Record<T>::members());
  if constexpr (std::is_same_v<T, SoundnessOptions>) allowed.insert("variant_index");
  for (const auto& [name, value] : root.fields) { (void)value; if (!allowed.contains(name)) invalid("unknown field " + name); }
  T result{};
  if constexpr (std::is_same_v<T, SoundnessOptions>) if (field(root, "variant") && field(root, "variant_index")) invalid("variant and variant_index conflict by presence");
  std::apply([&](const auto&... entry) { ([&] { if (const auto* value = field(root, entry.name)) {
    using M = std::remove_cvref_t<decltype(result.*(entry.pointer))>; result.*(entry.pointer) = read_value<M>(*value);
  } }(), ...); }, Record<T>::members());
  if constexpr (std::is_same_v<T, SoundnessOptions>) if (const auto* value = field(root, "variant_index")) result.variant = static_cast<SoundnessVariant>(integer<std::int32_t>(*value));
  if (supplied) { std::set<std::string> names; for (const auto& [name, value] : root.fields) { (void)value; names.insert(name); } *supplied = std::move(names); }
  return result;
}
template<class T> void write(std::ostream& out, const T& value) {
  if constexpr (std::is_same_v<T, bool>) out << (value ? "true" : "false");
  else if constexpr (std::is_integral_v<T>) out << value;
  else if constexpr (std::is_same_v<T, double>) { const auto text = serialize_embedding_workbench_vector_json({value}); out.write(text.data() + 1, static_cast<std::streamsize>(text.size() - 2)); }
  else if constexpr (std::is_same_v<T, std::string>) out << q(value);
  else if constexpr (std::is_enum_v<T>) { const auto name = enum_name(value); out << "{\"name\":" << (name.empty() ? "null" : q(name)) << ",\"index\":" << static_cast<std::int32_t>(value) << '}'; }
  else if constexpr (Optional<T>::value) { if (value) write(out, *value); else out << "null"; }
  else if constexpr (std::is_same_v<T, std::vector<double>>) out << serialize_embedding_workbench_vector_json(value);
  else if constexpr (Vector<T>::value) { out << '['; bool first = true; for (const auto& item : value) { if (!first) out << ','; first = false; write(out, item); } out << ']'; }
  else { out << '{'; bool first = true; std::apply([&](const auto&... entry) { ([&] { if (!first) out << ','; first = false; out << q(entry.name) << ':'; write(out, value.*(entry.pointer)); }(), ...); }, Record<T>::members()); out << '}'; }
}
template<class T> std::string render(const T& value) { auto out = output(); write(out, value); return out.str(); }
std::string hex(std::string_view value) { constexpr char digits[] = "0123456789abcdef"; std::string result; for (const unsigned char c : value) { result += digits[c >> 4]; result += digits[c & 15]; } return result; }
std::string carrier(std::string_view dataset, std::string_view bytes) {
  const bool utf8 = q(bytes).starts_with('"');
  return "{\"dataset\":" + q(dataset) + ",\"encoding\":" + q(utf8 ? "utf8" : "hex") + ",\"data\":" + q(utf8 ? bytes : std::string_view(hex(bytes))) + ",\"bytes\":" + std::to_string(bytes.size()) + ",\"sha256\":" + q(footprints_workbench_sha256(bytes)) + '}';
}
std::string exception_type(const std::exception& error) {
  if (dynamic_cast<const std::invalid_argument*>(&error)) return "invalid_argument";
  if (dynamic_cast<const std::out_of_range*>(&error)) return "out_of_range";
  if (dynamic_cast<const std::overflow_error*>(&error)) return "overflow_error";
  if (dynamic_cast<const std::length_error*>(&error)) return "length_error";
  if (dynamic_cast<const std::runtime_error*>(&error)) return "runtime_error";
  return "exception";
}
std::string error_json(std::string_view phase, const std::exception& error) { return "{\"phase\":" + q(phase) + ",\"exception_type\":" + q(exception_type(error)) + ",\"message\":" + q(error.what()) + '}'; }
std::string strings(const std::set<std::string>& values) { return render(std::vector<std::string>(values.begin(), values.end())); }
using Options = std::variant<std::monostate, MarkingEquationOptions, ExtendedMarkingEquationOptions, SoundnessOptions>;
using Result = std::variant<std::monostate, WorkflowMarkings, MarkingEquationResult, ExtendedMarkingEquationResult, WorkflowNetResult, bool, SoundnessResult>;
std::string options_json(const Options& value) { return std::visit([](const auto& v) -> std::string { if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::monostate>) return "null"; else return render(v); }, value); }
std::string result_type(const Result& value) { static constexpr const char* names[] = {"", "WorkflowMarkings", "MarkingEquationResult", "ExtendedMarkingEquationResult", "WorkflowNetResult", "bool", "SoundnessResult"}; return names[value.index()]; }
std::string result_json(const Result& value) { return std::visit([](const auto& v) -> std::string { if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::monostate>) return "null"; else return render(v); }, value); }

PetriAnalysisWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts, const std::vector<DiscoveryWorkbenchPart>& query, const std::optional<std::string>& transport_error = std::nullopt) {
  std::map<std::string, std::vector<const DiscoveryWorkbenchPart*>> by_name;
  std::string originals = "[", supplied_controls = "[";
  const auto add = [&](const DiscoveryWorkbenchPart& part, bool is_query) {
    by_name[part.name].push_back(&part);
    const bool upload = part.name == "model_file" || part.name == "trace_file";
    if (!is_query) { if (originals.size() > 1) originals += ','; originals += "{\"field\":" + q(part.name) + ",\"filename\":" + (part.file ? q(part.filename) : "null") + ",\"file\":" + (part.file ? "true" : "false") + ",\"original\":" + carrier(part.file ? part.filename : part.name, part.bytes) + '}'; }
    if (!upload || is_query) { if (supplied_controls.size() > 1) supplied_controls += ','; supplied_controls += "{\"name\":" + q(part.name) + ",\"origin\":" + q(is_query ? "query" : "multipart") + '}'; }
  };
  for (const auto& part : query) add(part, true);
  for (const auto& part : parts) add(part, false);
  originals += ']'; supplied_controls += ']';
  const auto find = [&](std::string_view name) -> const DiscoveryWorkbenchPart* { const auto found = by_name.find(std::string(name)); return found == by_name.end() || found->second.size() != 1 ? nullptr : found->second.front(); };
  const auto control = [&](std::string_view name, std::string_view fallback) { const auto* part = find(name); return part ? part->bytes : std::string(fallback); };
  const auto operation = control("operation", "analyze_soundness"), format = control("model_format", "exact");
  const bool extended = operation == "solve_extended_marking_equation";
  const std::optional<std::string> trace_format = find("trace_format") ? std::make_optional(find("trace_format")->bytes) : extended ? std::make_optional(std::string("native_string_array")) : std::nullopt;
  std::vector<std::string> parsed_controls;
  std::set<std::string> option_members, model_members;
  std::optional<PetriNet> model; std::optional<std::vector<std::string>> trace;
  Options configured; Result result;
  std::string configured_json = "null", before = "null", after = "null", trace_before = "null", trace_after = "null", result_value = "null", failure = "null", phase = "configuration_error", calls = "[", artifact_errors = "[";
  bool entered = false, returned = false;
  std::optional<bool> model_unchanged, trace_unchanged, options_unchanged;
  std::map<std::string, std::optional<std::string>> artifacts;
  for (const auto* name : {"source_net_json", "after_net_json", "trace_json", "configured_options_json", "result_json", "legacy_json"}) artifacts.emplace(name, std::nullopt);
  const auto call = [&](std::string_view name, std::string_view stage, std::string_view kind, const auto& function) {
    if (calls.size() > 1) calls += ',';
    const auto prefix = "{\"name\":" + q(name) + ",\"stage\":" + q(stage) + ",\"input_kind\":" + q(kind);
    try { function(); calls += prefix + ",\"returned\":true,\"error\":null}"; }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception& error) { calls += prefix + ",\"returned\":false,\"error\":" + error_json(stage == "export" ? "export_error" : "native_error", error) + '}'; throw; }
  };
  const auto artifact_error = [&](std::string_view name, std::string_view where, const std::exception& error) { if (artifact_errors.size() > 1) artifact_errors += ','; artifact_errors += "{\"name\":" + q(name) + ",\"error\":" + error_json(where, error) + '}'; };
  const auto after_snapshot = [&] {
    if (!entered) return;
    after = serialize_oc_playout_workbench_petri_net_json(*model); artifacts["after_net_json"] = after; model_unchanged = before == after;
    if (trace) { trace_after = render(*trace); trace_unchanged = trace_before == trace_after; }
    if (configured.index() != 0) options_unchanged = configured_json == options_json(configured);
  };
  try {
    if (transport_error) invalid(*transport_error);
    for (const auto& [name, values] : by_name) {
      if (values.size() != 1) invalid("duplicate control or upload: " + name);
      if (name != "operation" && name != "model_format" && name != "trace_format" && name != "options_json" && name != "model_file" && name != "trace_file") invalid("unknown control or upload: " + name);
    }
    for (const auto& part : query) if (part.name == "model_file" || part.name == "trace_file") invalid("model/trace uploads cannot be query controls");
    const bool marking = operation == "solve_marking_equation", soundness = operation == "analyze_soundness";
    const bool no_options = operation == "default_workflow_markings" || operation == "check_workflow_net" || operation == "is_workflow_net";
    if (!marking && !soundness && !extended && !no_options) invalid("unknown public operation");
    parsed_controls.push_back("operation");
    if (format != "exact") invalid("model_format must be exact"); parsed_controls.push_back("model_format");
    if (!extended && (find("trace_format") || find("trace_file"))) invalid("trace input is only accepted by solve_extended_marking_equation");
    if (extended) { if (*trace_format != "native_string_array") invalid("trace_format must be native_string_array"); parsed_controls.push_back("trace_format"); }
    if (no_options && find("options_json")) invalid("this public function has no options argument");
    const auto options_text = control("options_json", "{}");
    if (marking) configured = options<MarkingEquationOptions>(options_text, &option_members);
    else if (extended) configured = options<ExtendedMarkingEquationOptions>(options_text, &option_members);
    else if (soundness) configured = options<SoundnessOptions>(options_text, &option_members);
    if (!no_options) { configured_json = options_json(configured); artifacts["configured_options_json"] = configured_json; parsed_controls.push_back("options_json"); }
    phase = "preparation_error";
    const auto* file = find("model_file"); if (!file) invalid("model_file is required");
    model = parse_oc_playout_workbench_petri_net_json(file->bytes);
    const auto document = Parser(file->bytes).parse();
    for (const auto& [name, value] : document.fields) { (void)value; model_members.insert(name); }
    before = serialize_oc_playout_workbench_petri_net_json(*model); artifacts["source_net_json"] = before;
    if (extended) { const auto* word = find("trace_file"); if (!word) invalid("trace_file is required"); trace = parse_literal_string_vector_json(word->bytes); trace_before = render(*trace); artifacts["trace_json"] = trace_before; }
    phase = "native_error";
    const auto invoke = [&](std::string_view name, const auto& function) { call(name, "primary", extended ? "NativeString[]+PetriNet" : "PetriNet", [&] { entered = true; function(); returned = true; }); };
    try {
      if (operation == "default_workflow_markings") invoke("default_workflow_markings(net)", [&] { result = default_workflow_markings(*model); });
      else if (marking) invoke("solve_marking_equation(net, options)", [&] { result = solve_marking_equation(*model, std::get<MarkingEquationOptions>(configured)); });
      else if (extended) invoke("solve_extended_marking_equation(trace, net, options)", [&] { result = solve_extended_marking_equation(*trace, *model, std::get<ExtendedMarkingEquationOptions>(configured)); });
      else if (operation == "check_workflow_net") invoke("check_workflow_net(net)", [&] { result = check_workflow_net(*model); });
      else if (operation == "is_workflow_net") invoke("is_workflow_net(net)", [&] { result = is_workflow_net(*model); });
      else invoke("analyze_soundness(net, options)", [&] { result = analyze_soundness(*model, std::get<SoundnessOptions>(configured)); });
    } catch (...) { after_snapshot(); throw; }
    after_snapshot();
    phase = "result_serialization_error";
    try { result_value = result_json(result); artifacts["result_json"] = "{\"type\":" + q(result_type(result)) + ",\"value\":" + result_value + '}'; }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception& error) { artifact_error("result_json", phase, error); failure = error_json(phase, error); }
    std::visit([&](const auto& value) { using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, MarkingEquationResult> || std::is_same_v<T, ExtendedMarkingEquationResult> || std::is_same_v<T, WorkflowNetResult> || std::is_same_v<T, SoundnessResult>) {
        try { call("petri_analysis_json(result)", "export", result_type(result), [&] { artifacts["legacy_json"] = petri_analysis_json(value); }); }
        catch (const std::bad_alloc&) { throw; }
        catch (const std::exception& error) { artifact_error("legacy_json", "export_error", error); }
      }
    }, result);
  } catch (const std::bad_alloc&) { throw; }
  catch (const std::exception& error) { failure = error_json(phase, error); }
  calls += ']'; artifact_errors += ']';
  std::string artifact_map = "{";
  for (const auto& [name, bytes] : artifacts) { if (artifact_map.size() > 1) artifact_map += ','; artifact_map += q(name) + ':' + (bytes ? carrier(name + ".json", *bytes) : "null"); }
  artifact_map += '}';
  const auto maybe = [](const std::optional<bool>& flag) { return flag ? (*flag ? "true" : "false") : "null"; };
  const auto* submitted = find("options_json");
  const auto trace_holder = extended ? "{\"before\":" + trace_before + ",\"after\":" + trace_after + '}' : "null";
  const auto result_holder = returned && result_value != "null" ? "{\"type\":" + q(result_type(result)) + ",\"value\":" + result_value + '}' : "null";
  PetriAnalysisWorkbenchResponse response; response.status = failure == "null" || returned ? 200 : 400;
  response.json = "{\"family\":\"ORIGINAL_PETRI_ANALYSIS_WORKBENCH\",\"version\":\"original-petri-analysis-workbench-v1\",\"operation\":" + q(operation)
      + ",\"executed\":" + (returned ? "true" : "false") + ",\"native_entered\":" + (entered ? "true" : "false") + ",\"native_returned\":" + (returned ? "true" : "false")
      + ",\"native_calls\":" + calls + ",\"error\":" + failure + ",\"originals\":" + originals + ",\"parsed_controls\":" + render(parsed_controls) + ",\"supplied_controls\":" + supplied_controls
      + ",\"input_context\":{\"size_t_bytes\":" + std::to_string(sizeof(std::size_t)) + ",\"model_format\":" + q(format) + ",\"trace_format\":" + (trace_format ? q(*trace_format) : "null") + ",\"model_supplied_members\":" + strings(model_members)
      + ",\"model_unchanged\":" + maybe(model_unchanged) + ",\"trace_unchanged\":" + maybe(trace_unchanged) + ",\"options_unchanged\":" + maybe(options_unchanged) + '}'
      + ",\"model\":{\"before\":" + before + ",\"after\":" + after + "},\"trace\":" + trace_holder + ",\"submitted_options\":" + (submitted ? carrier(submitted->file ? submitted->filename : submitted->name, submitted->bytes) : "null")
      + ",\"configured_options\":" + configured_json + ",\"supplied_options\":" + strings(option_members) + ",\"result\":" + result_holder + ",\"artifact_bytes\":" + artifact_map + ",\"artifact_errors\":" + artifact_errors + '}';
  return response;
}
}  // namespace

MarkingEquationOptions parse_petri_analysis_workbench_marking_options_json(std::string_view value, std::set<std::string>* supplied) { return options<MarkingEquationOptions>(value, supplied); }
ExtendedMarkingEquationOptions parse_petri_analysis_workbench_extended_options_json(std::string_view value, std::set<std::string>* supplied) { return options<ExtendedMarkingEquationOptions>(value, supplied); }
SoundnessOptions parse_petri_analysis_workbench_soundness_options_json(std::string_view value, std::set<std::string>* supplied) { return options<SoundnessOptions>(value, supplied); }
std::string serialize_petri_analysis_workbench_options_json(const MarkingEquationOptions& value) { return render(value); }
std::string serialize_petri_analysis_workbench_options_json(const ExtendedMarkingEquationOptions& value) { return render(value); }
std::string serialize_petri_analysis_workbench_options_json(const SoundnessOptions& value) { return render(value); }
std::string serialize_petri_analysis_workbench_result_json(const WorkflowMarkings& value) { return render(value); }
std::string serialize_petri_analysis_workbench_result_json(const MarkingEquationResult& value) { return render(value); }
std::string serialize_petri_analysis_workbench_result_json(const ExtendedMarkingEquationResult& value) { return render(value); }
std::string serialize_petri_analysis_workbench_result_json(const WorkflowNetResult& value) { return render(value); }
std::string serialize_petri_analysis_workbench_result_json(const SoundnessResult& value) { return render(value); }
std::string serialize_petri_analysis_workbench_result_json(bool value) { return render(value); }
PetriAnalysisWorkbenchResponse execute_petri_analysis_workbench(const std::vector<DiscoveryWorkbenchPart>& parts, const std::vector<DiscoveryWorkbenchPart>& query) { return execute(parts, query); }
PetriAnalysisWorkbenchResponse execute_petri_analysis_workbench_multipart(std::string_view content_type, std::string_view body, const std::vector<DiscoveryWorkbenchPart>& query, bool transport_complete) {
  if (!transport_complete) return execute({}, query, "multipart transport did not complete");
  std::vector<DiscoveryWorkbenchPart> parts;
  try { parts = parse_discovery_workbench_multipart(content_type, body); }
  catch (const std::bad_alloc&) { throw; }
  catch (const std::exception& error) { return execute({}, query, error.what()); }
  return execute(parts, query);
}
}  // namespace minera
