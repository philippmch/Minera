// Replays the PM4Py 2.7.22 case-relations bank (docs/case-relations-bank.json.gz)
// against the native typed-table merge.
#include "case_relations_bank_cases.hpp"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
using namespace case_relations_bank;

std::string index_mismatch(const EventTable& actual, const BankCase& item) {
  const auto count = actual.rows.size();
  for (std::size_t row = 0; row < count; ++row) {
    std::int64_t label = static_cast<std::int64_t>(row);
    if (actual.index) {
      const auto* cell = std::get_if<EventAttribute>(&actual.index->labels[row].value);
      const auto* value = cell ? std::get_if<std::int64_t>(&cell->typed_value) : nullptr;
      if (!value) return "index label is not an integer at " + std::to_string(row);
      label = *value;
    }
    if (label != item.index[row]) return "index label differs at " + std::to_string(row);
  }
  return {};
}

// Returns an empty string when the native merge matches the bank entry.
std::string merge_case(const BankCase& item) {
  EventTable actual;
  try {
    actual = merge_case_relations(item.base, item.incoming, item.relations,
                                  options_for(item.config));
  } catch (const std::invalid_argument& error) {
    return item.raises ? std::string{} : std::string{"native raised: "} + error.what();
  }
  if (item.raises) return "native returned, reference raised";
  try {
    reference_check(item.name, actual, item.expected);
  } catch (const std::exception& error) {
    return error.what();
  }
  if (!actual.dtypes.empty() && actual.dtypes != item.expected.dtypes) {
    for (std::size_t col = 0; col < actual.dtypes.size(); ++col)
      if (actual.dtypes[col] != item.expected.dtypes[col])
        return "dtype differs at " + item.expected.columns[col] + ": native " +
               actual.dtypes[col] + ", reference " + item.expected.dtypes[col];
  }
  return index_mismatch(actual, item);
}
}  // namespace

int main() {
  std::size_t total = 0, raising = 0, failures = 0;
  for (const auto& item : cases()) {
    ++total;
    if (item.raises) ++raising;
    const auto failure = merge_case(item);
    if (failure.empty()) continue;
    ++failures;
    std::cerr << item.name << " [" << item.config << "]: " << failure << '\n';
  }
  std::cout << total << " calls compared (" << raising << " raising)\n";
  if (failures || total < 440 || raising < 46) {
    std::cerr << failures << " mismatches\n";
    return 1;
  }
  return 0;
}
