// Runs the Minera core on a CSV event log and emits the analysis (event log
// plus discovered directly-follows graph) as JSON for the frontend.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/alpha_miner.hpp"
#include "minera/bpmn.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/decision_mining.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/model_decoration.hpp"
#include "minera/performance.hpp"
#include "minera/pnml.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"

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
               "  --inductive-variant <id>  log (default: split the log and\n"
               "                            recurse) or dfg (IMd: recurse on\n"
               "                            the directly-follows graph alone,\n"
               "                            scaling to huge logs)\n"
               "  --alpha-variant <id>      plus (default: short loops become\n"
               "                            self loops and two-way causality)\n"
               "                            or classic (the textbook Alpha)\n"
               "  --dependency-threshold <f>  Heuristic Miner: minimum dependency\n"
               "                            measure for an edge (default: 0.9)\n"
               "  --and-threshold <f>       Heuristic Miner: minimum AND measure\n"
               "                            for a parallel split/join (default:\n"
               "                            0.65)\n"
               "  --min-edge-frequency <n>  Heuristic Miner: direct successions\n"
               "                            an edge needs (default: 1)\n"
               "  --pnml <path>             Also write a model as PNML to <path>\n"
               "  --check-pnml <path>       Also check the log against the\n"
               "                            workflow net in this PNML file\n"
               "                            (conformance id: imported)\n"
               "  --pnml-miner <id>         Which model to export as PNML:\n"
               "                            inductive (default), alpha, or heuristic\n"
               "  --bpmn <path>             Also write a model as BPMN 2.0 to <path>\n"
               "  --check-bpmn <path>       Also check the log against the\n"
               "                            BPMN 2.0 model in this file\n"
               "                            (conformance id: imported)\n"
               "  --bpmn-miner <id>         Which model to export as BPMN:\n"
               "                            inductive (default), alpha, or heuristic\n"
               "  --xes <path>              Also write the event log (after any\n"
               "                            filtering) as XES to <path>\n"
               "  --decisions <miner>       Mine the decision points of one\n"
               "                            model (inductive, alpha, or\n"
               "                            heuristic): which attribute values\n"
               "                            steer a case down which branch;\n"
               "                            embedded under \"decisions\"\n"
               "\n  Log filtering (applied before discovery):\n"
               "  --time-from <iso8601>     Keep only behaviour at/after this instant\n"
               "  --time-to <iso8601>       Keep only behaviour at/before this instant\n"
               "  --time-mode <mode>        How the time range reads a trace:\n"
               "                            events (default: keep events in range),\n"
               "                            traces_contained, or traces_intersecting\n"
               "  --keep-activities <a,b>   Keep only these activities\n"
               "  --remove-activities <a,b> Drop these activities\n"
               "  --keep-resources <a,b>    Keep only events by these resources\n"
               "  --remove-resources <a,b>  Drop events by these resources\n"
               "  --keep-lifecycles <a,b>   Keep only events with one of these\n"
               "                            lifecycle transitions (e.g.\n"
               "                            complete, to make a start/complete\n"
               "                            log atomic for discovery)\n"
               "  --remove-lifecycles <a,b> Drop events with these transitions\n"
               "  --attribute-key <k>       Filter on this event attribute (an\n"
               "                            extra CSV column or non-standard\n"
               "                            XES key); alone it keeps only\n"
               "                            events carrying the attribute\n"
               "  --keep-attribute-values <a,b>   Keep only events whose\n"
               "                            attribute has one of these values\n"
               "  --remove-attribute-values <a,b> Drop events with these values\n"
               "  --attribute-mode <mode>   events (default: filter events) or\n"
               "                            traces (keep/drop whole cases by\n"
               "                            the same attribute match)\n"
               "  --case-attribute-key <k>  Filter cases on this case attribute\n"
               "                            (an XES trace attribute or a\n"
               "                            `case:` CSV column); alone it keeps\n"
               "                            only cases carrying the attribute\n"
               "  --keep-case-attribute-values <a,b>   Keep only cases whose\n"
               "                            attribute has one of these values\n"
               "  --remove-case-attribute-values <a,b> Drop cases with these\n"
               "                            values\n"
               "  --rework-activity <a>     Keep only traces where <a> repeats\n"
               "  --rework-min <n>          Repetitions that count as rework\n"
               "                            (default: 2)\n"
               "  --keep-directly-follows <a->b,c->d>\n"
               "                            Keep only cases where some listed\n"
               "                            activity is directly followed by\n"
               "                            its partner\n"
               "  --remove-directly-follows <a->b,...>  Drop such cases\n"
               "  --keep-eventually-follows <a->b,...>  Like the directly-\n"
               "                            follows pair, but the partner may\n"
               "                            occur anywhere later in the case\n"
               "  --remove-eventually-follows <a->b,...>  Drop such cases\n"
               "  --prefix-activity <a>     Keep each case's head up to (and\n"
               "                            including) the first <a>; cases\n"
               "                            without <a> are dropped\n"
               "  --suffix-activity <a>     Keep each case's tail from the\n"
               "                            first <a> on; cases without <a>\n"
               "                            are dropped\n"
               "  --between-start <a>       With --between-end: cut each trace\n"
               "  --between-end <b>         into the subtraces running from <a>\n"
               "                            to <b>, endpoints included\n"
               "  --start-activities <a,b>  Keep traces starting with one of these\n"
               "  --end-activities <a,b>    Keep traces ending with one of these\n"
               "  --min-length <n>          Drop traces shorter than n events\n"
               "  --max-length <n>          Drop traces longer than n events\n"
               "  --min-variant-frequency <n>  Keep variants seen at least n times\n"
               "  --top-variants <n>        Keep only the n most frequent variants\n"
               "  --variant-coverage <f>    Keep the most frequent variants\n"
               "                            that together cover this share of\n"
               "                            cases, f in (0,1]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  std::filesystem::path pnml_output;
  std::filesystem::path check_pnml;
  std::filesystem::path check_bpmn;
  std::filesystem::path bpmn_output;
  std::filesystem::path xes_output;
  std::string pnml_miner = "inductive";
  std::string bpmn_miner = "inductive";
  std::string decisions_miner;  // empty = no decision mining
  minera::CsvImportOptions options;
  minera::LogFilterOptions filter;
  minera::InductiveMinerOptions inductive_options;
  minera::AlphaMinerOptions alpha_options;
  minera::HeuristicMinerOptions heuristic_options;

  // Succession pairs are parsed while the try block below is not yet open, so
  // report a malformed value as a clean usage error rather than a terminate.
  const auto parse_pairs = [](const char* value) {
    try {
      return minera::parse_activity_pairs(value);
    } catch (const std::exception& error) {
      std::cerr << "error: " << error.what() << '\n';
      std::exit(2);
    }
  };

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
    } else if (arg == "--check-pnml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      check_pnml = value;
    } else if (arg == "--check-bpmn") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      check_bpmn = value;
    } else if (arg == "--bpmn") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      bpmn_output = value;
    } else if (arg == "--xes") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      xes_output = value;
    } else if (arg == "--pnml-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pnml_miner = value;
    } else if (arg == "--bpmn-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      bpmn_miner = value;
    } else if (arg == "--decisions") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      decisions_miner = value;
    } else if (arg == "--noise-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      inductive_options.noise_threshold = std::atof(value);
    } else if (arg == "--inductive-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant != "log" && variant != "dfg") return usage(argv[0]);
      inductive_options.variant = variant == "dfg"
                                      ? minera::InductiveMinerVariant::kDfg
                                      : minera::InductiveMinerVariant::kLog;
    } else if (arg == "--alpha-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant != "classic" && variant != "plus") return usage(argv[0]);
      alpha_options.short_loops = variant == "plus";
    } else if (arg == "--dependency-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.dependency_threshold = std::atof(value);
    } else if (arg == "--and-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.and_threshold = std::atof(value);
    } else if (arg == "--min-edge-frequency") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.min_frequency =
          static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--delimiter") {
      const auto* value = next();
      if (!value || value[0] == '\0') return usage(argv[0]);
      options.delimiter = value[0];
    } else if (arg == "--time-from") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.time_from = value;
    } else if (arg == "--time-to") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.time_to = value;
    } else if (arg == "--time-mode") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto mode = minera::parse_time_filter_mode(value);
      if (!mode) {
        std::cerr << "error: unknown --time-mode '" << value
                  << "' (expected events, traces_contained, or "
                     "traces_intersecting)\n";
        return 2;
      }
      filter.time_mode = *mode;
    } else if (arg == "--keep-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_activities = split_csv(value);
    } else if (arg == "--remove-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_activities = split_csv(value);
    } else if (arg == "--keep-resources") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_resources = split_csv(value);
    } else if (arg == "--remove-resources") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_resources = split_csv(value);
    } else if (arg == "--keep-lifecycles") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_lifecycles = split_csv(value);
    } else if (arg == "--remove-lifecycles") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_lifecycles = split_csv(value);
    } else if (arg == "--attribute-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.attribute_key = value;
    } else if (arg == "--keep-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_attribute_values = split_csv(value);
    } else if (arg == "--remove-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_attribute_values = split_csv(value);
    } else if (arg == "--attribute-mode") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto mode = minera::parse_attribute_filter_mode(value);
      if (!mode) {
        std::cerr << "error: unknown --attribute-mode '" << value
                  << "' (expected events or traces)\n";
        return 2;
      }
      filter.attribute_mode = *mode;
    } else if (arg == "--case-attribute-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.case_attribute_key = value;
    } else if (arg == "--keep-case-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_case_attribute_values = split_csv(value);
    } else if (arg == "--remove-case-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_case_attribute_values = split_csv(value);
    } else if (arg == "--rework-activity") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.rework_activity = value;
    } else if (arg == "--rework-min") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.rework_min_occurrences =
          static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--keep-directly-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_directly_follows = parse_pairs(value);
    } else if (arg == "--remove-directly-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_directly_follows = parse_pairs(value);
    } else if (arg == "--keep-eventually-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_eventually_follows = parse_pairs(value);
    } else if (arg == "--remove-eventually-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_eventually_follows = parse_pairs(value);
    } else if (arg == "--prefix-activity") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.prefix_activity = value;
    } else if (arg == "--suffix-activity") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.suffix_activity = value;
    } else if (arg == "--between-start") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.between_start = value;
    } else if (arg == "--between-end") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.between_end = value;
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
    } else if (arg == "--variant-coverage") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.variant_coverage = std::atof(value);
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
    auto graph = minera::discover_directly_follows_graph(log);
    const auto tree = minera::discover_process_tree(log, inductive_options);
    const auto net = minera::discover_petri_net(log, alpha_options);
    const auto heuristics =
        minera::discover_heuristics_net(log, heuristic_options);
    const auto performance = minera::compute_performance_metrics(log, graph);
    const auto variant_durations =
        minera::compute_variant_durations(log, graph);
    const auto resources = minera::discover_resource_network(log);
    const auto eventually = minera::discover_eventually_follows(log, graph);

    // Check every discovered model both ways, keyed by miner id. The export is
    // offline, so embed the alignments too (the server defers them instead).
    const auto inductive_net = minera::to_petri_net(tree);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const auto model = [&](std::string id, const minera::PetriNet& petri) {
      return minera::ModelConformance{
          std::move(id), minera::final_marking_reachable(petri),
          minera::check_conformance(log, petri),
          minera::check_footprints(log, petri),
          minera::check_alignment(log, petri)};
    };
    std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net)};

    // Optionally check the log against a user-supplied model too — a PNML
    // workflow net or a BPMN 2.0 process, both landing under the single
    // "imported" conformance id. Its activities are registered in the id
    // registry first, so the ones the log never shows still resolve to names
    // in the export.
    if (!check_pnml.empty() && !check_bpmn.empty()) {
      std::cerr << "error: pass either --check-pnml or --check-bpmn, not "
                   "both (they share the conformance id \"imported\")\n";
      return 2;
    }
    if (!check_pnml.empty() || !check_bpmn.empty()) {
      const auto imported = check_pnml.empty()
                                ? minera::import_bpmn(check_bpmn)
                                : minera::import_pnml(check_pnml);
      for (const auto& activity : imported.activities()) {
        graph.ensure_activity(activity);
      }
      conformance.push_back(model("imported", imported));
    }
    const auto dataset = input.filename().string();

    // Optionally mine the decision points of one model's net.
    std::optional<minera::DecisionMiningResult> decisions;
    if (!decisions_miner.empty()) {
      const minera::PetriNet* selected = nullptr;
      if (decisions_miner == "inductive") {
        selected = &inductive_net;
      } else if (decisions_miner == "alpha") {
        selected = &net;
      } else if (decisions_miner == "heuristic") {
        selected = &heuristic_net;
      } else {
        std::cerr << "error: unknown --decisions miner '" << decisions_miner
                  << "' (expected inductive, alpha, or heuristic)\n";
        return 2;
      }
      decisions = minera::mine_decisions(log, *selected);
      decisions->miner = decisions_miner;
    }
    const minera::DecisionMiningResult* decisions_ptr =
        decisions ? &*decisions : nullptr;

    // Frequency/performance annotations for the Petri-net model view.
    const auto petri_decoration = minera::decorate_model(log, net);

    if (output.empty()) {
      minera::write_analysis_json(std::cout, dataset, log, graph, &tree, &net,
                                  &heuristics, &conformance, &performance,
                                  &resources, &variant_durations, &eventually,
                                  decisions_ptr, &petri_decoration);
    } else {
      std::ofstream file(output);
      if (!file) {
        std::cerr << "error: cannot open output file " << output << '\n';
        return 1;
      }
      minera::write_analysis_json(file, dataset, log, graph, &tree, &net,
                                  &heuristics, &conformance, &performance,
                                  &resources, &variant_durations, &eventually,
                                  decisions_ptr, &petri_decoration);
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

    // Optionally emit one discovered model as BPMN. The inductive tree maps
    // block by block; the other miners' workflow nets convert by their token
    // semantics.
    if (!bpmn_output.empty()) {
      std::ofstream bpmn_file(bpmn_output);
      if (!bpmn_file) {
        std::cerr << "error: cannot open BPMN output file " << bpmn_output
                  << '\n';
        return 1;
      }
      if (bpmn_miner == "inductive") {
        minera::write_bpmn(bpmn_file, tree, dataset);
      } else if (bpmn_miner == "alpha") {
        minera::write_bpmn(bpmn_file, net, dataset);
      } else if (bpmn_miner == "heuristic") {
        minera::write_bpmn(bpmn_file, heuristic_net, dataset);
      } else {
        std::cerr << "error: unknown --bpmn-miner '" << bpmn_miner
                  << "' (expected inductive, alpha, or heuristic)\n";
        return 1;
      }
    }

    // Optionally emit the (filtered) event log as standard XES, making the
    // CLI double as a CSV-to-XES converter.
    if (!xes_output.empty()) {
      std::ofstream xes_file(xes_output);
      if (!xes_file) {
        std::cerr << "error: cannot open XES output file " << xes_output
                  << '\n';
        return 1;
      }
      minera::write_xes(xes_file, log, dataset);
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
