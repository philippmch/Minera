#pragma once

#include "minera/event_log.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace minera {

inline constexpr const char* kDeclareExistence = "existence";
inline constexpr const char* kDeclareExactlyOne = "exactly_one";
inline constexpr const char* kDeclareInit = "init";
inline constexpr const char* kDeclareRespondedExistence = "responded_existence";
inline constexpr const char* kDeclareResponse = "response";
inline constexpr const char* kDeclarePrecedence = "precedence";
inline constexpr const char* kDeclareSuccession = "succession";
inline constexpr const char* kDeclareAltResponse = "altresponse";
inline constexpr const char* kDeclareAltPrecedence = "altprecedence";
inline constexpr const char* kDeclareAltSuccession = "altsuccession";
inline constexpr const char* kDeclareChainResponse = "chainresponse";
inline constexpr const char* kDeclareChainPrecedence = "chainprecedence";
inline constexpr const char* kDeclareChainSuccession = "chainsuccession";
inline constexpr const char* kDeclareAbsence = "absence";
inline constexpr const char* kDeclareCoexistence = "coexistence";
inline constexpr const char* kDeclareNonCoexistence = "noncoexistence";
inline constexpr const char* kDeclareNonSuccession = "nonsuccession";
inline constexpr const char* kDeclareNonChainSuccession = "nonchainsuccession";

struct DeclareConstraint {
  std::string first;
  std::string second;
  bool binary = false;
  int support = 0;
  int confidence = 0;
};

struct DeclareModel {
  // All 18 families are present (possibly empty), in lexical-independent
  // PM4Py order. Rules within a family are ordered by activity keys.
  std::map<std::string, std::vector<DeclareConstraint>> rules;

  DeclareModel();
  void add(std::string family, DeclareConstraint constraint);
  [[nodiscard]] std::size_t size() const noexcept;
};

struct DeclareDiscoveryOptions {
  // nullopt means PM4Py's default (infer activities / enable all templates);
  // an engaged empty vector is an explicit empty filter.
  std::optional<std::vector<std::string>> considered_activities;
  std::optional<std::vector<std::string>> allowed_templates;
  double min_support_ratio = 0.0;
  double min_confidence_ratio = 0.0;
  bool has_min_support_ratio = false;
  bool has_min_confidence_ratio = false;
  double auto_selection_multiplier = 0.8;
};

[[nodiscard]] DeclareModel discover_declare(
    const EventLog& log, const DeclareDiscoveryOptions& options = {});

struct DeclareDeviation {
  std::string template_name;
  std::string first;
  std::string second;
  bool binary = false;
};

struct DeclareConformance {
  std::vector<DeclareDeviation> deviations;
  int no_constr_total = 0;
  int no_dev_total = 0;
  double dev_fitness = 1.0;
  bool is_fit = true;
};

[[nodiscard]] DeclareConformance check_declare_conformance(
    const std::vector<std::string>& trace, const DeclareModel& model);
[[nodiscard]] DeclareConformance check_declare_conformance(
    const Trace& trace, const DeclareModel& model);
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance(
    const EventLog& log, const DeclareModel& model);

}  // namespace minera
