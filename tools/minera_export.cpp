// Runs the Minera core on a CSV event log and emits the analysis (event log
// plus discovered directly-follows graph) as JSON for the frontend.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/performance.hpp"
#include "minera/pnml.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace {

bool ends_with_ci(std::string_view value, std::string_view suffix) {
  if (value.size() < suffix.size()) return false;
  const auto tail = value.substr(value.size() - suffix.size());
  return std::equal(tail.begin(), tail.end(), suffix.begin(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == b;
  });
}

// Splits a comma-separated option value into trimmed, non-empty activity names.
std::vector<std::string> split_csv(std::string_view value) {
  std::vector<std::string> parts;
  std::string current;
  for (const char ch : value) {
    if (ch == ',') {
      if (!current.empty()) parts.push_back(current);
      current.clear();
    } else if (ch != ' ' || !current.empty()) {
      current.push_back(ch);
    }
  }
  if (!current.empty()) parts.push_back(current);
  return parts;
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " <event_log.csv|.xes> [options]\n"
               "  A .xes file is imported as XES; any other file as CSV.\n"
               "  -o <path>                 Write JSON to <path> instead of stdout\n"
               "  --case-column <name>      Case id column (default: case_id)\n"
               "  --activity-column <name>  Activity column (default: activity)\n"
               "  --timestamp-column <name> Timestamp column (default: timestamp)\n"
               "  --resource-column <name>  Resource column (default: resource)\n"
               "  --delimiter <char>        Field delimiter (default: ,)\n"
               "  --noise-threshold <f>     Inductive Miner – infrequent filter\n"
               "                            in [0,1] (default: 0 = classic)\n"
               "  --pnml <path>             Also write a model as PNML to <path>\n"
               "  --pnml-miner <id>         Which model to export as PNML:\n"
               "                            inductive (default), alpha, or heuristic\n"
               "\n  Log filtering (applied before discovery):\n"
               "  --keep-activities <a,b>   Keep only these activities\n"
               "  --remove-activities <a,b> Drop these activities\n"
               "  --start-activities <a,b>  Keep traces starting with one of these\n"
               "  --end-activities <a,b>    Keep traces ending with one of these\n"
               "  --min-length <n>          Drop traces shorter than n events\n"
               "  --max-length <n>          Drop traces longer than n events\n"
               "  --min-variant-frequency <n>  Keep variants seen at least n times\n"
               "  --top-variants <n>        Keep only the n most frequent variants\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  std::filesystem::path pnml_output;
  std::string pnml_miner = "inductive";
  minera::CsvImportOptions options;
  minera::LogFilterOptions filter;
  minera::InductiveMinerOptions inductive_options;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto next = [&]() -> const char* {
      return i + 1 < argc ? argv[++i] : nullptr;
    };

    if (arg == "-o") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      output = value;
    } else if (arg == "--case-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.case_id_column = value;
    } else if (arg == "--activity-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.activity_column = value;
    } else if (arg == "--timestamp-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.timestamp_column = value;
    } else if (arg == "--resource-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.resource_column = value;
    } else if (arg == "--pnml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pnml_output = value;
    } else if (arg == "--pnml-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pnml_miner = value;
    } else if (arg == "--noise-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      inductive_options.noise_threshold = std::atof(value);
    } else if (arg == "--delimiter") {
      const auto* value = next();
      if (!value || value[0] == '\0') return usage(argv[0]);
      options.delimiter = value[0];
    } else if (arg == "--keep-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_activities = split_csv(value);
    } else if (arg == "--remove-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_activities = split_csv(value);
    } else if (arg == "--start-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.start_activities = split_csv(value);
    } else if (arg == "--end-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.end_activities = split_csv(value);
    } else if (arg == "--min-length") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.min_length = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--max-length") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.max_length = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--min-variant-frequency") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.min_variant_frequency =
          static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--top-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.top_variants = static_cast<std::size_t>(std::atoll(value));
    } else if (!arg.empty() && arg.front() == '-') {
      return usage(argv[0]);
    } else if (input.empty()) {
      input = arg;
    } else {
      return usage(argv[0]);
    }
  }

  if (input.empty()) {
    return usage(argv[0]);
  }

  try {
    // A .xes file is read as XES; everything else is treated as CSV.
    auto log = ends_with_ci(input.string(), ".xes")
                   ? minera::import_xes_event_log(input)
                   : minera::import_csv_event_log(input, options);
    if (filter.any()) {
      log = minera::filter_event_log(log, filter);
    }
    const auto graph = minera::discover_directly_follows_graph(log);
    const auto tree = minera::discover_process_tree(log, inductive_options);
    const auto net = minera::discover_petri_net(log);
    const auto heuristics = minera::discover_heuristics_net(log);
    const auto performance = minera::compute_performance_metrics(log, graph);
    const auto variant_durations =
        minera::compute_variant_durations(log, graph);
    const auto resources = minera::discover_resource_network(log);

    // Check every discovered model both ways, keyed by miner id. The export is
    // offline, so embed the alignments too (the server defers them instead).
    const auto inductive_net = minera::to_petri_net(tree);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const auto model = [&](std::string id, const minera::PetriNet& petri) {
      return minera::ModelConformance{
          std::move(id), minera::final_marking_reachable(petri),
          minera::check_conformance(log, petri),
          minera::check_alignment(log, petri)};
    };
    const std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net)};
    const auto dataset = input.filename().string();

    if (output.empty()) {
      minera::write_analysis_json(std::cout, dataset, log, graph, &tree, &net,
                                  &heuristics, &conformance, &performance,
                                  &resources, &variant_durations);
    } else {
      std::ofstream file(output);
      if (!file) {
        std::cerr << "error: cannot open output file " << output << '\n';
        return 1;
      }
      minera::write_analysis_json(file, dataset, log, graph, &tree, &net,
                                  &heuristics, &conformance, &performance,
                                  &resources, &variant_durations);
    }

    // Optionally emit one discovered model as standard PNML.
    if (!pnml_output.empty()) {
      const minera::PetriNet* selected = nullptr;
      if (pnml_miner == "inductive") {
        selected = &inductive_net;
      } else if (pnml_miner == "alpha") {
        selected = &net;
      } else if (pnml_miner == "heuristic") {
        selected = &heuristic_net;
      } else {
        std::cerr << "error: unknown --pnml-miner '" << pnml_miner
                  << "' (expected inductive, alpha, or heuristic)\n";
        return 1;
      }
      std::ofstream pnml_file(pnml_output);
      if (!pnml_file) {
        std::cerr << "error: cannot open PNML output file " << pnml_output
                  << '\n';
        return 1;
      }
      minera::write_pnml(pnml_file, *selected, pnml_miner + " net");
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
