#pragma once

#include <istream>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct CaseRelation {
  std::string left_case_id;
  std::string right_case_id;
  friend bool operator==(const CaseRelation&, const CaseRelation&) = default;
};

enum class CaseRelationsVariant {
  kPandas,
  PANDAS = kPandas,
};

struct CaseRelationsOptions {
  CaseRelationsVariant variant = CaseRelationsVariant::kPandas;
  std::string timestamp_key = "time:timestamp";
};

[[nodiscard]] std::vector<CaseRelation> parse_case_relations_csv(
    std::istream& input, std::string left_column = "case_id_LEFT",
    std::string right_column = "case_id_RIGHT");

[[nodiscard]] EventLog merge_case_relations(
    const EventLog& base, const EventLog& incoming,
    const std::vector<CaseRelation>& relations,
    const CaseRelationsOptions& options = {});

}  // namespace minera
