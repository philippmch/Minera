#include <algorithm>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/live_stream.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " INPUT.csv [--thread-pool N] [--case-id-column NAME]"
               " [--activity-column NAME]\n";
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

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) return usage(argv[0]);
    minera::CsvImportOptions import_options;
    std::size_t thread_pool_size = 6;
    for (int i = 2; i < argc; ++i) {
      const std::string_view option(argv[i]);
      if (option == "--thread-pool" || option == "--case-id-column" ||
          option == "--activity-column") {
        if (++i >= argc || std::string_view(argv[i]).empty())
          return usage(argv[0]);
        if (option == "--thread-pool")
          thread_pool_size = parse_size(argv[i]);
        else if (option == "--case-id-column")
          import_options.case_id_column = argv[i];
        else
          import_options.activity_column = argv[i];
      } else {
        return usage(argv[0]);
      }
    }

    // The rows are a stream like the server's live sessions: every row is
    // kept with its literal case id and activity, in file order.
    import_options.keep_na_first_class_values = true;
    const auto log = minera::import_csv_event_log(argv[1], import_options);
    std::vector<const minera::Event*> events;
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events) events.push_back(&event);
    std::stable_sort(events.begin(), events.end(),
                     [](const minera::Event* left, const minera::Event* right) {
                       return left->input_order < right->input_order;
                     });
    minera::LiveEventStream stream(thread_pool_size);
    std::atomic<std::size_t> received = 0;
    const auto observer_id =
        stream.register_observer([&received](const minera::Event&) { ++received; });
    stream.start();
    for (const auto* event : events) stream.append(*event);
    stream.stop();
    stream.deregister_observer(observer_id);

    std::cout << "{\"state\":\"finished\",\"events\":"
              << received.load() << "}\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
