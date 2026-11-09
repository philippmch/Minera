// Plays out a workflow net into a synthetic XES event log: simulate a model
// discovered or drawn anywhere — a PNML net or a BPMN 2.0 process — then feed
// the log back into Minera (or any other tool) to study or check it.

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/bpmn.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_net.hpp"
#include "minera/monte_carlo.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/ptml.hpp"
#include "minera/request_params.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " <model.pnml|model.bpmn|model.ptml> [options]\n"
               "  Simulates the workflow net and writes the traces as XES.\n"
               "  -o <path>          Write XES to <path> instead of stdout\n"
               "  --traces <n>       Cases to generate (default: 1000, as pm4py);\n"
               "                     with --extensive, the cap on distinct\n"
               "                     traces (default: uncapped up to 1000);\n"
               "                     with --monte-carlo, the simulation count\n"
               "                     (default: 100)\n"
               "  --max-steps <n>    Firing cap per case (default: 1000)\n"
               "  --seed <n>         RNG seed; same seed, same log (default: 1)\n"
               "  --extensive        Enumerate every distinct trace the model\n"
               "                     allows (shortest first) instead of\n"
               "                     sampling randomly\n"
               "  --max-length <n>   Extensive: longest visible trace to\n"
               "                     enumerate (default: 10)\n"
               "  --return-elements <bool> Extensive visited JSON output\n"
               "  --max-marking-occ <n> Extensive marking repetition bound\n"
               "  --initial-marking <n,...>  Explicit starting marker vector\n"
               "  --final-marking <n,...>    Explicit accepted marker vector\n"
               "  --dfg              Treat input CSV/XES as a log and play out its DFG\n"
               "  --variant <name>   DFG variant: classic or performance\n"
               "  --return-variants <bool> Return classic variant counts as JSON\n"
               "  --return-only-if-complete <bool> Suppress incomplete results\n"
               "  --min-variant-occurrences <n> Minimum variant count\n"
               "  --interrupt-when-dfg-complete <bool> Stop after graph coverage\n"
               "  --add-trace-if-takes-new-edges <bool> Keep edge-covering traces\n"
               "  --max-variants <n> DFG classic: variant cap (default: 3000)\n"
               "  --min-weighted-probability <f> DFG classic: stop once the\n"
               "                     variants cover this probability (default: 1)\n"
               "  --max-occurrences-per-activity <n> DFG classic: repeats of one\n"
               "                     activity per trace (default: 2)\n"
               "  --max-execution-time-ms <n> DFG classic: time cap (default: 0,\n"
               "                     none)\n"
               "  --dfg-traces <n>   DFG performance: cases (default: 1000)\n"
               "  --dfg-seed <n>     DFG performance: RNG seed (default: 1)\n"
               "  --case-arrival-rate <f> DFG performance: seconds between case\n"
               "                     starts (default: 1)\n"
               "  --max-events-per-trace <n> DFG performance: event cap per case\n"
               "                     (default: 1000)\n"
               "  --start-time <ts>  DFG performance: first case start (default:\n"
               "                     epoch second 10000000)\n"
               "                     --dfg refuses --traces and --seed (in any\n"
               "                     order); use --dfg-traces and --dfg-seed.\n"
               "  --monte-carlo      Use bounded virtual-time Monte Carlo\n"
               "  --monte-carlo-simulations <n> Case count\n"
               "  --monte-carlo-activity-key <name> Generated activity attribute\n"
               "  --monte-carlo-timestamp-key <name> Generated timestamp attribute\n"
               "  --monte-carlo-initial-marking <n,...> Initial marker vector\n"
               "  --monte-carlo-final-marking <n,...> Final marker vector\n"
               "  --monte-carlo-max-steps <n> Firing cap\n"
               "  --monte-carlo-seed <u64> RNG seed\n"
               "  --monte-carlo-case-arrival-ratio <f> Arrival ratio\n"
               "  --monte-carlo-case-arrival-seconds <f> Arrival interval\n"
               "  --monte-carlo-start <f> First case arrival epoch seconds\n"
               "  --monte-carlo-default-duration-seconds <f> Default duration\n"
               "  --monte-carlo-force-distribution <name> Fitted duration family\n"
               "  --monte-carlo-transition-weights <w,...> Transition weights\n"
               "  --monte-carlo-transition-durations <d:v,...> Durations\n"
               "  --monte-carlo-resources-per-place <p=n,...> Resource pools\n"
               "  --monte-carlo-default-resources-per-place <n> Default pool size\n"
               "  --monte-carlo-enable-diagnostics <bool> Include diagnostics\n"
               "  --monte-carlo-stop-at-final <bool> Stop at final marking\n"
               "  --monte-carlo-provided-stochastic-map <records|empty> Explicit map\n"
               "  --monte-carlo-diagnostic-interval <n> Diagnostic cadence\n"
               "  --monte-carlo-small-scale-factor <f> Timing scale factor\n"
               "  --monte-carlo-max-thread-execution-time <f> Runtime cap\n"
               "  --monte-carlo-token-replay-variant <forward|backwards> Replay mode\n"
               "  --stochastic-petri      Emit tangible CTMC JSON\n"
               "  --stochastic-petri-rates <r,...> Exponential rates\n"
               "  --stochastic-petri-initial-marking <n,...> Initial marking\n"
               "  --stochastic-petri-immediate-transitions <n,...> Immediate indices\n"
               "  --stochastic-petri-max-states <n> Reachability state cap\n"
               "  --stochastic-petri-analysis <name> reachability, transient, or steady_state\n"
               "  --stochastic-petri-time <f> Transient time in seconds\n"
               "  --stochastic-petri-initial-state <n> Transient source state\n"
               "  --stochastic-petri-initial-probabilities <p,...> Transient vector\n"
               "  A repeated option keeps its last value.\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  minera::PlayoutOptions options;
  // pm4py's basic Petri playout generates 1000 traces by default.
  options.traces = 1000;
  bool traces_supplied = false;
  bool seed_supplied = false;
  minera::ExtensivePlayoutOptions extensive_options;
  bool extensive = false;
  bool extensive_control_used = false;
  bool dfg = false;
  bool monte_carlo = false;
  bool stochastic_petri = false;
  std::map<std::string, std::string> stochastic_petri_fields;
  std::map<std::string, std::string> dfg_fields;
  std::map<std::string, std::string> monte_carlo_fields;
  const std::set<std::string> monte_carlo_field_names = {
      "monte_carlo_simulations", "monte_carlo_activity_key",
      "monte_carlo_timestamp_key", "monte_carlo_initial_marking",
      "monte_carlo_final_marking", "monte_carlo_max_steps",
      "monte_carlo_seed", "monte_carlo_case_arrival_ratio",
      "monte_carlo_case_arrival_seconds", "monte_carlo_start",
      "monte_carlo_default_duration_seconds",
      "monte_carlo_force_distribution", "monte_carlo_transition_weights",
      "monte_carlo_transition_durations",
      "monte_carlo_resources_per_place",
      "monte_carlo_default_resources_per_place",
      "monte_carlo_enable_diagnostics", "monte_carlo_stop_at_final",
      "monte_carlo_diagnostic_interval", "monte_carlo_small_scale_factor",
      "monte_carlo_max_thread_execution_time",
      "monte_carlo_token_replay_variant",
      "monte_carlo_numpy_legacy_seed", "monte_carlo_numpy_choice_order",
      "monte_carlo_provided_stochastic_map"};
  const auto parse_marking = [](const char* value, const char* option) {
    if (!value || *value == '\0')
      throw std::invalid_argument(std::string(option) +
                                  " expects a non-empty marking");
    std::vector<std::uint32_t> result;
    const std::string text(value);
    std::size_t begin = 0;
    for (std::size_t index = 0; index <= text.size(); ++index) {
      if (index != text.size() && text[index] != ',') continue;
      const auto token = text.substr(begin, index - begin);
      if (token.empty())
        throw std::invalid_argument(std::string(option) +
                                    " expects comma-separated counts");
      errno = 0;
      char* end = nullptr;
      const auto parsed = std::strtoull(token.c_str(), &end, 10);
      if (end != token.c_str() + token.size() || errno == ERANGE ||
          parsed > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument(std::string(option) +
                                    " expects uint32 counts");
      result.push_back(static_cast<std::uint32_t>(parsed));
      begin = index + 1;
    }
    return result;
  };

  // Repeated flags follow the usual command-line rule: the last one wins.
  const auto strict_u64 = [](const char* value, std::string_view option)
      -> std::optional<std::uint64_t> {
    try {
      return minera::parse_strict_u64(value, option);
    } catch (const std::exception& error) {
      std::cerr << "error: " << error.what() << '\n';
      return std::nullopt;
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
    } else if (arg == "--traces") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parsed = strict_u64(value, arg);
      if (!parsed) return 2;
      options.traces = static_cast<std::size_t>(*parsed);
      traces_supplied = true;
    } else if (arg == "--max-steps") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parsed = strict_u64(value, arg);
      if (!parsed) return 2;
      options.max_steps = static_cast<std::size_t>(*parsed);
    } else if (arg == "--seed") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parsed = strict_u64(value, arg);
      if (!parsed) return 2;
      options.seed = *parsed;
      seed_supplied = true;
    } else if (arg == "--extensive") {
      extensive = true;
    } else if (arg == "--dfg") {
      dfg = true;
    } else if (arg == "--monte-carlo") {
      monte_carlo = true;
    } else if (arg == "--stochastic-petri") {
      stochastic_petri = true;
    } else if (arg == "--stochastic-petri-rates" ||
               arg == "--stochastic-petri-initial-marking" ||
               arg == "--stochastic-petri-immediate-transitions" ||
               arg == "--stochastic-petri-max-states" ||
               arg == "--stochastic-petri-analysis" ||
               arg == "--stochastic-petri-time" ||
               arg == "--stochastic-petri-initial-state" ||
               arg == "--stochastic-petri-initial-probabilities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      stochastic_petri_fields[key] = value;
      stochastic_petri = true;
    } else if (arg.rfind("--monte-carlo-", 0) == 0) {
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      if (!monte_carlo_field_names.contains(key)) {
        std::cerr << "error: unknown Monte Carlo option: --" << key << '\n';
        return 2;
      }
      const auto* value = next();
      if (!value) return usage(argv[0]);
      monte_carlo_fields.insert_or_assign(std::move(key), value);
      monte_carlo = true;
    } else if (arg == "--variant") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      dfg_fields["dfg_playout_variant"] = value;
    } else if (arg == "--dfg-traces" || arg == "--max-variants" ||
               arg == "--min-weighted-probability" ||
               arg == "--max-occurrences-per-activity" ||
               arg == "--max-execution-time-ms" || arg == "--case-arrival-rate" ||
               arg == "--max-events-per-trace" || arg == "--start-time" ||
               arg == "--dfg-seed" || arg == "--return-variants" ||
               arg == "--return-only-if-complete" ||
               arg == "--min-variant-occurrences" ||
               arg == "--interrupt-when-dfg-complete" ||
               arg == "--add-trace-if-takes-new-edges") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      const std::string key = arg == "--dfg-traces" ? "dfg_playout_traces" :
          arg == "--max-variants" ? "dfg_playout_max_variants" :
          arg == "--min-weighted-probability" ? "dfg_playout_min_weighted_probability" :
          arg == "--max-occurrences-per-activity" ? "dfg_playout_max_occurrences_per_activity" :
          arg == "--max-execution-time-ms" ? "dfg_playout_max_execution_time_ms" :
          arg == "--case-arrival-rate" ? "dfg_playout_case_arrival_rate" :
          arg == "--max-events-per-trace" ? "dfg_playout_max_events_per_trace" :
          arg == "--dfg-seed" ? "dfg_playout_seed" :
          arg == "--start-time" ? "dfg_playout_start_time" :
          arg == "--return-variants" ? "dfg_playout_return_variants" :
          arg == "--return-only-if-complete" ? "dfg_playout_return_only_if_complete" :
          arg == "--min-variant-occurrences" ? "dfg_playout_min_variant_occurrences" :
          arg == "--interrupt-when-dfg-complete" ? "dfg_playout_interrupt_when_dfg_complete" :
          "dfg_playout_add_trace_if_takes_new_edges";
      dfg_fields[key] = value;
    } else if (arg == "--max-length") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parsed = strict_u64(value, arg);
      if (!parsed) return 2;
      extensive_options.max_length = static_cast<std::size_t>(*parsed);
    } else if (arg == "--return-elements") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view parsed(value);
      if (parsed == "true" || parsed == "1") {
        extensive_options.return_elements = true;
      } else if (parsed == "false" || parsed == "0") {
        extensive_options.return_elements = false;
      } else {
        std::cerr << "error: --return-elements expects true or false\n";
        return 2;
      }
      extensive_control_used = true;
    } else if (arg == "--max-marking-occ") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parsed = strict_u64(value, arg);
      if (!parsed) return 2;
      extensive_options.max_marking_occ = static_cast<std::size_t>(*parsed);
      extensive_control_used = true;
    } else if (arg == "--initial-marking" || arg == "--final-marking") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parsed = parse_marking(
          value, arg == "--initial-marking" ? "--initial-marking"
                                            : "--final-marking");
      if (arg == "--initial-marking") {
        options.initial_marking = parsed;
        extensive_options.initial_marking = std::move(parsed);
      } else {
        options.final_marking = parsed;
        extensive_options.final_marking = std::move(parsed);
      }
    } else if (!arg.empty() && arg.front() == '-') {
      return usage(argv[0]);
    } else if (input.empty()) {
      input = arg;
    } else {
      return usage(argv[0]);
    }
  }
  if (input.empty() || options.traces == 0 || options.max_steps == 0 ||
      extensive_options.max_length == 0) {
    return usage(argv[0]);
  }
  // Whichever comes first, --traces and --seed configure the Petri net
  // playout only; with --dfg they would be ignored silently.
  if (dfg && (traces_supplied || seed_supplied)) {
    std::cerr << "error: --dfg does not take --traces or --seed; use --dfg-traces and --dfg-seed\n";
    return 2;
  }
  if (stochastic_petri && (dfg || monte_carlo || extensive)) {
    std::cerr << "error: --stochastic-petri cannot be combined with another playout mode\n";
    return 2;
  }
  minera::StochasticPetriOptions stochastic_petri_options;
  if (stochastic_petri) {
    const auto lookup = [&stochastic_petri_fields](std::string_view name) {
      const auto it = stochastic_petri_fields.find(std::string(name));
      return it == stochastic_petri_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&stochastic_petri_fields](std::string_view name) {
      return stochastic_petri_fields.contains(std::string(name));
    };
    try {
      stochastic_petri_options =
          minera::parse_stochastic_petri_params(lookup, present);
    } catch (const std::exception& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  if (extensive_control_used && !extensive) {
    std::cerr << "error: extensive controls require --extensive\n";
    return 2;
  }
  // pm4py's extensive playout has no trace cap; keep the native distinct
  // trace budget unless --traces asks for fewer.
  if (traces_supplied) extensive_options.max_traces = options.traces;

  try {
    if (dfg) {
      std::ifstream input_file(input, std::ios::binary);
      if (!input_file) throw std::runtime_error("cannot open input log");
      auto lower_path = input.string();
      std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(),
                     [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                     });
      const auto source = (lower_path.ends_with(".xes") ||
                           lower_path.ends_with(".xes.gz"))
                              ? minera::import_xes_dataframe_log(input_file)
                              : minera::import_csv_event_log(input_file);
      const auto frequency = minera::discover_frequency_dfg(source);
      minera::DfgActivityCounts starts, ends;
      for (const auto& trace : source.traces) {
        if (!trace.events.empty()) {
          ++starts[trace.events.front().activity];
          ++ends[trace.events.back().activity];
        }
      }
      const auto lookup = [&dfg_fields](std::string_view name) {
        const auto it = dfg_fields.find(std::string(name));
        return it == dfg_fields.end() ? std::string{} : it->second;
      };
      const auto dfg_options = minera::parse_dfg_playout_params(lookup);
      std::optional<minera::DfgPerformance> performance;
      if (dfg_options.variant == minera::DfgPlayoutVariant::kPerformance) {
        // A CSV or XES input, which pm4py discovers as a DataFrame.
        minera::DfgStatisticalOptions dataframe_options;
        dataframe_options.dataframe_arithmetic = true;
        performance = minera::discover_performance_dfg(source, dataframe_options);
      }
      const auto result = minera::dfg_playout_result(
          frequency, starts, ends, dfg_options,
          performance ? &*performance : nullptr);
      const auto name = input.stem().string() + " dfg playout";
      if (output.empty()) {
        if (dfg_options.return_variants &&
            dfg_options.variant == minera::DfgPlayoutVariant::kClassic)
          std::cout << minera::dfg_playout_json(result) << '\n';
        else
          minera::write_xes(std::cout, result.log, name);
      } else {
        std::ofstream file(output);
        if (!file) throw std::runtime_error("cannot open output file");
        if (dfg_options.return_variants &&
            dfg_options.variant == minera::DfgPlayoutVariant::kClassic)
          file << minera::dfg_playout_json(result) << '\n';
        else
          minera::write_xes(file, result.log, name);
      }
      return 0;
    }
    const auto net =
        input.extension() == ".bpmn" ? minera::import_bpmn(input)
        : input.extension() == ".ptml"
            ? minera::to_petri_net(minera::import_ptml(input))
            : minera::import_pnml(input);
    if (stochastic_petri) {
      const auto reachability =
          minera::stochastic_petri_reachability(net, stochastic_petri_options);
      std::string json;
      if (stochastic_petri_options.analysis ==
          minera::StochasticPetriAnalysisVariant::kReachability) {
        json = minera::serialize_stochastic_petri_json(reachability);
      } else if (stochastic_petri_options.analysis ==
                 minera::StochasticPetriAnalysisVariant::kTransient) {
        const auto initial_state =
            stochastic_petri_options.analysis_initial_state.has_value()
                ? stochastic_petri_options.analysis_initial_state
                : reachability.initial_state;
        const auto& derived_probabilities = reachability.initial_probabilities;
        const bool has_derived_probabilities =
            !derived_probabilities.empty() &&
            derived_probabilities.size() == reachability.states.size();
        if (stochastic_petri_options.analysis_initial_probabilities.empty() &&
            !initial_state.has_value() && !has_derived_probabilities)
          throw std::invalid_argument(
              "stochastic Petri transient analysis could not retain the initial state; provide an initial probability vector");
        const auto distribution =
            !stochastic_petri_options.analysis_initial_probabilities.empty()
                ? minera::stochastic_petri_transient(
                      reachability,
                      stochastic_petri_options.analysis_time_seconds,
                      stochastic_petri_options.analysis_initial_probabilities)
                : initial_state.has_value()
                      ? minera::stochastic_petri_transient(
                            reachability,
                            stochastic_petri_options.analysis_time_seconds,
                            *initial_state)
                      : minera::stochastic_petri_transient(
                            reachability,
                            stochastic_petri_options.analysis_time_seconds,
                            derived_probabilities);
        json = minera::serialize_stochastic_petri_distribution_json(distribution);
      } else {
        json = minera::serialize_stochastic_petri_distribution_json(
            minera::stochastic_petri_steady_state(reachability));
      }
      if (output.empty()) std::cout << json << '\n';
      else {
        std::ofstream file(output);
        if (!file) throw std::runtime_error("cannot open output file");
        file << json << '\n';
      }
      return 0;
    }
    minera::EventLog log;
    if (monte_carlo) {
      if (!monte_carlo_fields.contains("monte_carlo_simulations"))
        // pm4py's Monte Carlo default is 100 simulations; --traces still
        // overrides it.
        monte_carlo_fields["monte_carlo_simulations"] =
            std::to_string(traces_supplied ? options.traces : 100);
      if (!monte_carlo_fields.contains("monte_carlo_max_steps"))
        monte_carlo_fields["monte_carlo_max_steps"] =
            std::to_string(options.max_steps);
      if (!monte_carlo_fields.contains("monte_carlo_seed"))
        monte_carlo_fields["monte_carlo_seed"] =
            std::to_string(options.seed);
      const auto lookup = [&monte_carlo_fields](std::string_view name) {
        const auto it = monte_carlo_fields.find(std::string(name));
        return it == monte_carlo_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&monte_carlo_fields](std::string_view name) {
        return monte_carlo_fields.contains(std::string(name));
      };
      const auto mc_options =
          minera::parse_monte_carlo_params(lookup, present);
      log = minera::monte_carlo(net, mc_options).log;
    } else {
      log = extensive ? minera::extensive_playout(net, extensive_options)
                       : minera::playout(net, options);
    }
    const auto name = input.stem().string() + " playout";

    if (extensive && extensive_options.return_elements) {
      const auto json = minera::serialize_extensive_playout_visited_json(
          net, minera::extensive_playout_visited(net, extensive_options));
      if (output.empty()) {
        std::cout << json << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << json << '\n';
      }
    } else if (output.empty()) {
      minera::write_xes(std::cout, log, name);
    } else {
      std::ofstream file(output);
      if (!file) {
        std::cerr << "error: cannot open output file " << output << '\n';
        return 1;
      }
      minera::write_xes(file, log, name);
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
