#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "minera/conformance.hpp"
#include "minera/petri_usage_comparison.hpp"
#include "minera/pnml.hpp"

namespace minera::tools {

// The legacy conformance bundle counts repeated unit endpoints. Retain compact
// weighted models for independent supported routes without publishing invented
// conformance scores or treating an unavailable reachability check as unsound.
[[nodiscard]] inline std::optional<ModelConformance> pnml_conformance_weight_gate(
    const PetriNet& net, std::string miner = "imported") {
  if (!net.has_compact_ordinary_arcs()) return std::nullopt;
  ModelConformance result;
  result.miner = std::move(miner);
  result.computed = false;
  result.error = "Conformance for zero, signed or large ordinary arc weights is not available yet.";
  return result;
}

template <typename Integer>
[[nodiscard]] std::vector<Integer> pnml_marking(
    const TokenMarking& marking) {
  static_assert(std::is_integral_v<Integer> && !std::is_same_v<Integer, bool>);
  std::vector<Integer> result;
  result.reserve(marking.size());
  for (const auto tokens : marking) {
    if (tokens < 0 || tokens > std::numeric_limits<Integer>::max()) {
      throw std::invalid_argument("PNML marking exceeds supported token range");
    }
    result.push_back(static_cast<Integer>(tokens));
  }
  return result;
}

// Reachability and footprints in the interactive conformance bundle still
// use bounded markings. Keep the document available to exact token evaluators.
[[nodiscard]] inline std::optional<ModelConformance> pnml_conformance_gate(
    const PetriNet& net, const PnmlDocument* document = nullptr,
    std::string miner = "imported") {
  if (auto gated = pnml_conformance_weight_gate(net, miner)) return gated;
  if (!document) return std::nullopt;
  const auto outside = [](const TokenMarking& marking) {
    return std::any_of(marking.begin(), marking.end(), [](const auto value) {
      return value < 0 || value > std::numeric_limits<int>::max();
    });
  };
  if (!outside(document->initial_marking) && !outside(document->final_marking))
    return std::nullopt;
  ModelConformance result;
  result.miner = std::move(miner);
  result.computed = false;
  result.error = "Conformance for markings outside the native int range is not available yet.";
  return result;
}

// The interactive supplied-model adapters retain general markings. For older
// documents without marking keys, keep the unique structural-boundary default.
// A recorded zero is an explicit marking and must never trigger inference.
inline PnmlDocument resolve_pnml_model(PnmlDocument document,
                                      bool infer_legacy_boundaries = true) {
  if (!infer_legacy_boundaries) return document;
  const auto count = document.net.place_count();
  std::vector<bool> incoming(count, false), outgoing(count, false);
  for (const auto& transition : document.net.transitions()) {
    for (const auto place : transition.outputs) incoming.at(place) = true;
    for (const auto place : transition.inputs) outgoing.at(place) = true;
    for (const auto place : transition.reset_inputs) outgoing.at(place) = true;
    for (const auto place : transition.inhibitor_inputs) outgoing.at(place) = true;
  }
  const auto infer = [&](TokenMarking& marking,
                         std::vector<bool>& keys,
                         const std::vector<bool>& connected, bool initial) {
    if (std::any_of(keys.begin(), keys.end(), [](bool key) { return key; }))
      return;
    const auto boundary = std::find(connected.begin(), connected.end(), false);
    if (boundary == connected.end() ||
        std::find(boundary + 1, connected.end(), false) != connected.end())
      return;
    const auto place = static_cast<std::size_t>(boundary - connected.begin());
    marking.at(place) = 1;
    keys.at(place) = true;
    if (initial) document.net.set_source(place);
    else document.net.set_sink(place);
  };
  infer(document.initial_marking, document.initial_marking_keys, incoming, true);
  infer(document.final_marking, document.final_marking_keys, outgoing, false);
  return document;
}

inline PnmlImportOptions pnml_model_import_options(bool public_markings = false) {
  PnmlImportOptions options;
  options.infer_initial_marking = false;
  options.infer_final_marking = false;
  options.require_workflow_boundaries = false;
  options.import_stochastic_map = true;
  options.public_marking_semantics = public_markings;
  return options;
}

[[nodiscard]] inline PnmlDocument parse_pnml_model(std::string_view xml,
                                                bool public_markings = false) {
  return resolve_pnml_model(
      parse_pnml_document(xml, pnml_model_import_options(public_markings)),
      !public_markings);
}

[[nodiscard]] inline PnmlDocument import_pnml_model(
    const std::filesystem::path& path, bool public_markings = false) {
  return resolve_pnml_model(
      import_pnml_document(path, pnml_model_import_options(public_markings)),
      !public_markings);
}

[[nodiscard]] inline TokenReplayOptions pnml_replay_options(
    const PnmlDocument& document, TokenReplayOptions base = {}) {
  base.exact_initial_marking = TokenMarking(document.initial_marking);
  base.exact_final_marking = TokenMarking(document.final_marking);
  // Keep legacy vectors available to callers for representable PNML inputs.
  // Exact vectors carry larger counts directly into replay.
  const auto fits_legacy = [](const auto& marking) {
    return std::all_of(marking.begin(), marking.end(), [](const auto value) {
      return value >= 0 && value <= std::numeric_limits<int>::max();
    });
  };
  base.initial_marking = fits_legacy(document.initial_marking)
      ? pnml_marking<int>(document.initial_marking) : std::vector<int>{};
  base.final_marking = fits_legacy(document.final_marking)
      ? pnml_marking<int>(document.final_marking) : std::vector<int>{};
  base.initial_marking_keys = document.initial_marking_keys;
  base.final_marking_keys = document.final_marking_keys;
  base.place_names = document.place_ids;
  base.transition_names = document.transition_ids;
  return base;
}

[[nodiscard]] inline AlignmentOptions pnml_alignment_options(
    const PnmlDocument& document, AlignmentOptions base = {}) {
  base.initial_marking = pnml_marking<int>(document.initial_marking);
  base.final_marking = pnml_marking<int>(document.final_marking);
  return base;
}

// Fitting and CTMC analysis share a caller-supplied initial marking. Imported
// IDs and the independently supplied final marking still guide token replay.
[[nodiscard]] inline TokenReplayOptions stochastic_map_replay_options(
    TokenReplayOptions base, const std::vector<std::uint32_t>& initial_marking,
    const PnmlDocument* document = nullptr) {
  if (document) base = pnml_replay_options(*document, std::move(base));
  if (!initial_marking.empty()) {
    base.initial_marking.clear();
    base.exact_initial_marking.reset();
    base.initial_marking.reserve(initial_marking.size());
    for (const auto tokens : initial_marking) {
      if (tokens > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("stochastic map initial marking exceeds replay token range");
      base.initial_marking.push_back(static_cast<int>(tokens));
    }
    base.initial_marking_keys.clear();
  }
  return base;
}

// Signed fitted durations are meaningful log statistics, but a negative mean
// has no rate in the CTMC approximation. Preserve the fitted map and explain
// why this separate analysis could not be produced.
[[nodiscard]] inline StochasticPetriResult stochastic_map_reachability(
    const PetriNet& net, const StochasticMap& map,
    const StochasticMapFitOptions& fit, std::vector<std::uint32_t> initial_marking,
    std::size_t max_states) {
  try {
    const auto options = stochastic_map_to_options(
        net, map, fit, std::move(initial_marking), max_states);
    return stochastic_petri_reachability(net, options);
  } catch (const std::domain_error& error) {
    StochasticPetriResult result;
    result.exhausted = true;
    result.diagnostic = std::string("CTMC unavailable: ") + error.what();
    return result;
  }
}

[[nodiscard]] inline PetriUsageComparisonOptions pnml_comparison_options(
    const PnmlDocument& document, PetriUsageComparisonOptions base = {}) {
  base.initial_marking = pnml_marking<std::uint32_t>(document.initial_marking);
  base.final_marking = pnml_marking<std::uint32_t>(document.final_marking);
  base.initial_marking_keys = document.initial_marking_keys;
  base.final_marking_keys = document.final_marking_keys;
  base.place_names = document.place_ids;
  base.transition_names = document.transition_ids;
  return base;
}

}  // namespace minera::tools
