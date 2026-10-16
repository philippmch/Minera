#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "minera/evaluation.hpp"

namespace minera {
[[nodiscard]] StochasticLanguage parse_emd_workbench_language_json(std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_emd_workbench_language_json(const StochasticLanguage&);
// Only the four native value members are JSON options. Host functions stay
// empty until explicitly bound through the separately labelled table adapter.
[[nodiscard]] EmdOptions parse_emd_workbench_options_json(std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_emd_workbench_options_json(const EmdOptions&);
[[nodiscard]] std::string serialize_emd_workbench_callback_presence_json(const EmdOptions&);
[[nodiscard]] EmdResult parse_emd_workbench_result_json(std::string_view);
[[nodiscard]] std::string serialize_emd_workbench_result_json(const EmdResult&);
[[nodiscard]] std::string serialize_emd_workbench_scalar_json(double);
[[nodiscard]] bool parse_emd_workbench_normalize_json(std::string_view);

// Finite declarative host adapter; this does not serialize arbitrary C++
// function values. All ordered entries, including duplicate keys, are stored.
using EmdWorkbenchCallbackKey = std::variant<std::vector<std::string>, std::u32string, std::string>;
using EmdWorkbenchCallbackOutcome = std::variant<double, std::string>;
struct EmdWorkbenchCallbackEntry {
  EmdWorkbenchCallbackKey left, right;
  EmdWorkbenchCallbackOutcome outcome;
};
struct EmdWorkbenchCallbackTable {
  std::vector<EmdWorkbenchCallbackEntry> entries;
  std::optional<EmdWorkbenchCallbackOutcome> fallback;
};
// Slots: distance, encoded_distance, string_distance, utf8_string_distance.
struct EmdWorkbenchCallbackTables {
  std::array<std::optional<EmdWorkbenchCallbackTable>, 4> slots;
};
struct EmdWorkbenchCallbackInvocation {
  std::size_t slot = 0;
  EmdWorkbenchCallbackKey left, right;
  std::optional<std::size_t> entry_index;
  bool used_fallback = false;
  bool missing_entry = false;
  EmdWorkbenchCallbackOutcome outcome;
};
[[nodiscard]] EmdWorkbenchCallbackTables parse_emd_workbench_callback_tables_json(std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_emd_workbench_callback_tables_json(const EmdWorkbenchCallbackTables&);
[[nodiscard]] std::string serialize_emd_workbench_callback_invocations_json(const std::vector<EmdWorkbenchCallbackInvocation>&);
void bind_emd_workbench_callback_tables(EmdOptions&, const EmdWorkbenchCallbackTables&,
    std::shared_ptr<std::vector<EmdWorkbenchCallbackInvocation>>);
} // namespace minera
