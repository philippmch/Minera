#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "minera/live_stream.hpp"
#include "minera/streaming_import.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " INPUT.csv|INPUT.xes[.gz] [--trace] [--thread-pool N]\n";
  return 2;
}

std::size_t parse_size(std::string_view text) {
  if (text.empty()) throw std::invalid_argument("empty thread pool size");
  std::size_t value = 0;
  for (const char character : text) {
    if (character < '0' || character > '9')
      throw std::invalid_argument("invalid thread pool size");
    const auto digit = static_cast<std::size_t>(character - '0');
    if (value > (static_cast<std::size_t>(-1) - digit) / 10)
      throw std::invalid_argument("thread pool size is too large");
    value = value * 10 + digit;
  }
  return value;
}

bool ends_with_xes(std::string_view path) {
  const auto has_suffix = [path](std::string_view suffix) {
    if (path.size() < suffix.size()) return false;
    const auto offset = path.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
      char actual = path[offset + i];
      if (actual >= 'A' && actual <= 'Z')
        actual = static_cast<char>(actual - 'A' + 'a');
      if (actual != suffix[i]) return false;
    }
    return true;
  };
  return has_suffix(".xes") || has_suffix(".xes.gz");
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) return usage(argv[0]);
    bool trace_mode = false;
    std::size_t thread_pool_size = 6;
    for (int i = 2; i < argc; ++i) {
      const std::string_view option(argv[i]);
      if (option == "--trace") {
        trace_mode = true;
      } else if (option == "--thread-pool") {
        if (++i >= argc) return usage(argv[0]);
        thread_pool_size = parse_size(argv[i]);
      } else {
        return usage(argv[0]);
      }
    }
    if (trace_mode && !ends_with_xes(argv[1]))
      throw std::invalid_argument("--trace requires an XES input");

    if (trace_mode) {
      minera::XesTraceStreamReader reader(argv[1]);
      minera::LiveTraceStream stream(thread_pool_size);
      std::atomic<std::size_t> delivered = 0;
      const auto observer = stream.register_observer(
          [&delivered](const minera::Trace&) { ++delivered; });
      stream.start();
      const auto accepted = reader.to_trace_stream(stream);
      stream.stop();
      stream.deregister_observer(observer);
      std::cout << "{\"kind\":\"trace\",\"accepted\":" << accepted
                << ",\"delivered\":" << delivered.load()
                << ",\"state\":\"finished\"}\n";
    } else {
      const bool xes = ends_with_xes(argv[1]);
      std::atomic<std::size_t> delivered = 0;
      minera::LiveEventStream stream(thread_pool_size);
      const auto observer = stream.register_observer(
          [&delivered](const minera::Event&) { ++delivered; });
      stream.start();
      std::size_t accepted = 0;
      if (xes) {
        minera::XesEventStreamReader reader(argv[1]);
        accepted = reader.to_event_stream(stream);
      } else {
        minera::CsvEventStreamReader reader(argv[1]);
        accepted = reader.to_event_stream(stream);
      }
      stream.stop();
      stream.deregister_observer(observer);
      std::cout << "{\"kind\":\"event\",\"accepted\":" << accepted
                << ",\"delivered\":" << delivered.load()
                << ",\"state\":\"finished\"}\n";
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
