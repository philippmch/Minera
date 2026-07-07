// Tests for the server's request-parsing layer (tools/minera_server feeds these
// the raw form fields). Kept httplib-free so the parsing — importer selection,
// CSV-list splitting, error escaping, and option mapping — can be exercised
// directly without standing up an HTTP server.
#undef NDEBUG

#include "minera/request_params.hpp"

#include <cassert>
#include <map>
#include <stdexcept>
#include <string>

namespace {

// Builds a field lookup over a fixed map, mirroring how the server resolves a
// multipart form field (absent fields read as "").
minera::FieldLookup fields(std::map<std::string, std::string> values) {
  return [values = std::move(values)](std::string_view name) -> std::string {
    const auto it = values.find(std::string(name));
    return it == values.end() ? std::string{} : it->second;
  };
}

void ends_with_ci_is_case_insensitive() {
  assert(minera::ends_with_ci("LOG.XES", ".xes"));
  assert(minera::ends_with_ci("log.xes", ".xes"));
  assert(!minera::ends_with_ci("log.csv", ".xes"));
  assert(!minera::ends_with_ci("xe", ".xes"));  // shorter than the suffix
}

void looks_like_xes_uses_extension_then_content_sniff() {
  // Extension wins outright.
  assert(minera::looks_like_xes("trace.xes", "anything"));
  // A CSV name with XML content is still recognized by sniffing the preamble.
  assert(minera::looks_like_xes("trace.csv", "<?xml version=\"1.0\"?><log/>"));
  assert(minera::looks_like_xes("noext", "<log></log>"));
  // Leading whitespace before the preamble is tolerated.
  assert(minera::looks_like_xes("noext", "  \n\t<log/>"));
  // A UTF-8 BOM ahead of the preamble is skipped.
  assert(minera::looks_like_xes("noext", "\xEF\xBB\xBF<?xml ?>"));
  // Plain CSV is not XES.
  assert(!minera::looks_like_xes("trace.csv", "case,activity\n1,a\n"));
  assert(!minera::looks_like_xes("noext", "case,activity\n"));
}

void split_csv_trims_and_drops_empty_parts() {
  assert((minera::split_csv("a,b,c") ==
          std::vector<std::string>{"a", "b", "c"}));
  // Surrounding spaces are dropped on both sides; an internal space is kept.
  assert((minera::split_csv("a b, c") ==
          std::vector<std::string>{"a b", "c"}));
  assert((minera::split_csv("a , b ,c") ==
          std::vector<std::string>{"a", "b", "c"}));
  // Empty parts from doubled or trailing commas are skipped.
  assert((minera::split_csv("a,,b,") ==
          std::vector<std::string>{"a", "b"}));
  assert(minera::split_csv("").empty());
  assert(minera::split_csv(" , ").empty());
}

void error_json_escapes_quotes_and_backslashes() {
  assert(minera::error_json("plain") == "{\"error\": \"plain\"}");
  assert(minera::error_json("a\"b") == "{\"error\": \"a\\\"b\"}");
  assert(minera::error_json("a\\b") == "{\"error\": \"a\\\\b\"}");
}

void parse_mine_params_defaults_when_empty() {
  const auto params = minera::parse_mine_params(fields({}));
  // Stock importer defaults survive an empty form.
  assert(params.csv.case_id_column == "case_id");
  assert(params.csv.activity_column == "activity");
  assert(params.csv.delimiter == ',');
  assert(!params.filter.any());
  assert(params.inductive.noise_threshold == 0.0);
  assert(params.alpha.short_loops);  // Alpha+ is the default variant
}

void parse_mine_params_maps_csv_and_inductive_options() {
  const auto params = minera::parse_mine_params(fields({
      {"case_column", "Case"},
      {"activity_column", "Act"},
      {"timestamp_column", "Time"},
      {"resource_column", "Who"},
      {"lifecycle_column", "Phase"},
      {"delimiter", ";"},
      {"noise_threshold", "0.25"},
      {"alpha_variant", "classic"},
      {"inductive_variant", "dfg"},
      {"dependency_threshold", "0.75"},
      {"and_threshold", "0.5"},
      {"min_edge_frequency", "3"},
  }));
  assert(params.csv.case_id_column == "Case");
  assert(params.csv.activity_column == "Act");
  assert(params.csv.timestamp_column == "Time");
  assert(params.csv.resource_column == "Who");
  assert(params.csv.lifecycle_column == "Phase");
  assert(params.csv.delimiter == ';');  // only the first char is taken
  assert(params.inductive.noise_threshold == 0.25);
  assert(params.inductive.variant == minera::InductiveMinerVariant::kDfg);
  assert(!params.alpha.short_loops);
  assert(params.heuristic.dependency_threshold == 0.75);
  assert(params.heuristic.and_threshold == 0.5);
  assert(params.heuristic.min_frequency == 3);
}

void parse_mine_params_rejects_an_unknown_inductive_variant() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"inductive_variant", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_an_unknown_alpha_variant() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"alpha_variant", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_maps_filters() {
  const auto params = minera::parse_mine_params(fields({
      {"time_from", "2024-01-01"},
      {"time_to", "2024-01-31T23:59:59"},
      {"time_mode", "traces_intersecting"},
      {"keep_activities", "a, b ,c"},
      {"remove_activities", "x"},
      {"keep_resources", "Alice, Bob"},
      {"remove_resources", "Carol"},
      {"keep_lifecycles", "complete"},
      {"remove_lifecycles", "schedule, suspend"},
      {"start_activities", "a"},
      {"end_activities", "c"},
      {"min_length", "2"},
      {"max_length", "10"},
      {"min_variant_frequency", "3"},
      {"top_variants", "5"},
      {"rework_activity", "b"},
      {"rework_min_occurrences", "3"},
      {"between_start", "a"},
      {"between_end", "c"},
      {"attribute_key", "channel"},
      {"keep_attribute_values", "web, phone"},
      {"remove_attribute_values", "fax"},
      {"attribute_mode", "traces"},
      {"case_attribute_key", "region"},
      {"keep_case_attribute_values", "north, east"},
      {"remove_case_attribute_values", "south"},
  }));
  assert(params.filter.time_from == "2024-01-01");
  assert(params.filter.time_to == "2024-01-31T23:59:59");
  assert(params.filter.time_mode ==
         minera::TimeFilterMode::traces_intersecting);
  assert((params.filter.keep_activities ==
          std::vector<std::string>{"a", "b", "c"}));
  assert((params.filter.remove_activities == std::vector<std::string>{"x"}));
  assert((params.filter.keep_resources ==
          std::vector<std::string>{"Alice", "Bob"}));
  assert((params.filter.remove_resources == std::vector<std::string>{"Carol"}));
  assert((params.filter.keep_lifecycles ==
          std::vector<std::string>{"complete"}));
  assert((params.filter.remove_lifecycles ==
          std::vector<std::string>{"schedule", "suspend"}));
  assert((params.filter.start_activities == std::vector<std::string>{"a"}));
  assert((params.filter.end_activities == std::vector<std::string>{"c"}));
  assert(params.filter.min_length == 2);
  assert(params.filter.max_length == 10);
  assert(params.filter.min_variant_frequency == 3);
  assert(params.filter.top_variants == 5);
  assert(params.filter.rework_activity == "b");
  assert(params.filter.rework_min_occurrences == 3);
  assert(params.filter.between_start == "a");
  assert(params.filter.between_end == "c");
  assert(params.filter.attribute_key == "channel");
  assert((params.filter.keep_attribute_values ==
          std::vector<std::string>{"web", "phone"}));
  assert((params.filter.remove_attribute_values ==
          std::vector<std::string>{"fax"}));
  assert(params.filter.attribute_mode == minera::AttributeFilterMode::traces);
  assert(params.filter.case_attribute_key == "region");
  assert((params.filter.keep_case_attribute_values ==
          std::vector<std::string>{"north", "east"}));
  assert((params.filter.remove_case_attribute_values ==
          std::vector<std::string>{"south"}));
  assert(params.filter.any());
}

void parse_mine_params_rejects_an_unknown_attribute_mode() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"attribute_mode", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_an_unknown_time_mode() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"time_mode", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_ignores_blank_fields() {
  // A field present but empty must not override the default (the server treats
  // "" as "not supplied").
  const auto params = minera::parse_mine_params(fields({
      {"case_column", ""},
      {"delimiter", ""},
      {"min_length", ""},
  }));
  assert(params.csv.case_id_column == "case_id");
  assert(params.csv.delimiter == ',');
  assert(params.filter.min_length == 0);
}

}  // namespace

int main() {
  ends_with_ci_is_case_insensitive();
  looks_like_xes_uses_extension_then_content_sniff();
  split_csv_trims_and_drops_empty_parts();
  error_json_escapes_quotes_and_backslashes();
  parse_mine_params_defaults_when_empty();
  parse_mine_params_maps_csv_and_inductive_options();
  parse_mine_params_maps_filters();
  parse_mine_params_rejects_an_unknown_time_mode();
  parse_mine_params_rejects_an_unknown_attribute_mode();
  parse_mine_params_rejects_an_unknown_alpha_variant();
  parse_mine_params_rejects_an_unknown_inductive_variant();
  parse_mine_params_ignores_blank_fields();
  return 0;
}
