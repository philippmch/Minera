// Plays out a workflow net into a synthetic XES event log: simulate a model
// discovered or drawn anywhere — a PNML net or a BPMN 2.0 process — then feed
// the log back into Minera (or any other tool) to study or check it.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "minera/bpmn.hpp"
#include "minera/petri_net.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/ptml.hpp"
#include "minera/xes_export.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " <model.pnml|model.bpmn|model.ptml> [options]\n"
               "  Simulates the workflow net and writes the traces as XES.\n"
               "  -o <path>          Write XES to <path> instead of stdout\n"
               "  --traces <n>       Cases to generate (default: 100)\n"
               "  --max-steps <n>    Firing cap per case (default: 1000)\n"
               "  --seed <n>         RNG seed; same seed, same log (default: 1)\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  minera::PlayoutOptions options;

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
      options.traces = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--max-steps") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.max_steps = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--seed") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.seed = static_cast<std::uint64_t>(std::atoll(value));
    } else if (!arg.empty() && arg.front() == '-') {
      return usage(argv[0]);
    } else if (input.empty()) {
      input = arg;
    } else {
      return usage(argv[0]);
    }
  }
  if (input.empty() || options.traces == 0 || options.max_steps == 0) {
    return usage(argv[0]);
  }

  try {
    const auto net =
        input.extension() == ".bpmn" ? minera::import_bpmn(input)
        : input.extension() == ".ptml"
            ? minera::to_petri_net(minera::import_ptml(input))
            : minera::import_pnml(input);
    const auto log = minera::playout(net, options);
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
