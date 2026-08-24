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
               "  --traces <n>       Cases to generate (default: 100); with\n"
               "                     --extensive, the cap on distinct traces\n"
               "  --max-steps <n>    Firing cap per case (default: 1000)\n"
               "  --seed <n>         RNG seed; same seed, same log (default: 1)\n"
               "  --extensive        Enumerate every distinct trace the model\n"
               "                     allows (shortest first) instead of\n"
               "                     sampling randomly\n"
               "  --max-length <n>   Extensive: longest visible trace to\n"
               "                     enumerate (default: 10)\n"
               "  --initial-marking <n,...>  Explicit starting marker vector\n"
               "  --final-marking <n,...>    Explicit accepted marker vector\n"
               "  --dfg              Treat input CSV/XES as a log and play out its DFG\n"
               "  --variant <name>   DFG variant: classic or performance\n"
               "  --return-variants <bool> Return classic variant counts as JSON\n"
               "  --return-only-if-complete <bool> Suppress incomplete results\n"
               "  --min-variant-occurrences <n> Minimum variant count\n"
               "  --interrupt-when-dfg-complete <bool> Stop after graph coverage\n"
               "  --add-trace-if-takes-new-edges <bool> Keep edge-covering traces\n"
               "  --monte-carlo      Use bounded virtual-time Monte Carlo\n"
               "  --monte-carlo-simulations <n> Case count\n"
               "  --monte-carlo-max-steps <n> Firing cap\n"
               "  --monte-carlo-seed <u64> RNG seed\n"
               "  --monte-carlo-case-arrival-seconds <f> Arrival interval\n"
               "  --monte-carlo-default-duration-seconds <f> Default duration\n"
               "  --monte-carlo-transition-durations <d:v,...> Durations\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  minera::PlayoutOptions options;
  minera::ExtensivePlayoutOptions extensive_options;
  bool extensive = false;
  bool dfg = false;
  bool monte_carlo = false;
  std::map<std::string, std::string> dfg_fields;
  std::map<std::string, std::string> monte_carlo_fields;
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

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto next = [&]() -> const char* {
      return i + 1 < argc ? argv[++i] : nullptr;
    };

    if (arg == "-o") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      output = value;
    } else if (arg == "--traces" && !dfg) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.traces = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--max-steps") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.max_steps = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--seed" && !dfg) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.seed = static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--extensive") {
      extensive = true;
    } else if (arg == "--dfg") {
      dfg = true;
    } else if (arg == "--monte-carlo") {
      monte_carlo = true;
    } else if (arg.rfind("--monte-carlo-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
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
      extensive_options.max_length = static_cast<std::size_t>(std::atoll(value));
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
  extensive_options.max_traces = options.traces;

  try {
    if (dfg) {
      std::ifstream input_file(input);
      if (!input_file) throw std::runtime_error("cannot open input log");
      const auto source = input.extension() == ".xes"
                              ? minera::import_xes_event_log(input_file)
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
      if (dfg_options.variant == minera::DfgPlayoutVariant::kPerformance)
        performance = minera::discover_performance_dfg(source);
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
    minera::EventLog log;
    if (monte_carlo) {
      if (!monte_carlo_fields.contains("monte_carlo_simulations"))
        monte_carlo_fields["monte_carlo_simulations"] =
            std::to_string(options.traces);
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

    if (output.empty()) {
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
