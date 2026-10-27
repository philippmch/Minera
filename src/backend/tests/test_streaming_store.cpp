#include "minera/streaming_store.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
  const auto path = std::filesystem::temp_directory_path() /
                    "minera_streaming_dfg_store_test.csv";
  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  {
    minera::PersistentStreamingDfg store(path);
    // Reference: an empty case id or activity is an ordinary value.
    assert(store.receive("", "A"));
    assert(store.receive("case,1", "A"));
    assert(store.receive("case,1", "B"));
    assert(store.receive("case\"2", "A\nstart"));
    const auto snapshot = store.snapshot();
    assert(snapshot.events == 4);
    assert(snapshot.graph.at({"A", "B"}) == 1);
    assert(snapshot.start_activities.at("A") == 2);
    assert(snapshot.open_cases.at("") == "A");
    assert(snapshot.open_cases.at("case,1") == "B");
    assert(snapshot.open_cases.at("case\"2") == "A\nstart");
  }

  {
    minera::PersistentStreamingDfg reopened(path);
    const auto snapshot = reopened.snapshot();
    assert(snapshot.events == 4);
    assert(snapshot.graph.at({"A", "B"}) == 1);
    assert(snapshot.end_activities.at("A") == 1);
    assert(snapshot.end_activities.at("B") == 1);
    assert(snapshot.end_activities.at("A\nstart") == 1);
    reopened.reset();
    assert(reopened.snapshot().events == 0);
  }
  {
    minera::PersistentStreamingDfg empty(path);
    assert(empty.snapshot().events == 0);
    std::ifstream input(path, std::ios::binary);
    std::string header;
    std::getline(input, header);
    assert(header == "case_id,activity");
  }

  {
    std::ofstream invalid(path, std::ios::binary | std::ios::trunc);
    invalid << "wrong,header\n";
    invalid.close();
    bool rejected = false;
    try {
      minera::PersistentStreamingDfg invalid_store(path);
      (void)invalid_store;
    } catch (...) {
      rejected = true;
    }
    assert(rejected);
  }

  {
    minera::StreamingDictionaryOptions options;
    options.variant = minera::StreamingDictionaryVariant::kClassic;
    minera::StreamingDictionary dictionary(options);
    assert(dictionary.available());
    dictionary.set("b", "two");
    dictionary.set("a", "one");
    assert(dictionary.get("a") == std::optional<std::string>("one"));
    assert(dictionary.contains("b"));
    assert((dictionary.keys() == std::vector<std::string>{"a", "b"}));
    assert((dictionary.values() == std::vector<std::string>{"one", "two"}));
    assert(dictionary.size() == 2);
    assert(dictionary.erase("a"));
    assert(!dictionary.erase("a"));
    dictionary.clear();
    assert(dictionary.size() == 0);
    bool rejected = false;
    try {
      dictionary.set("", "value");
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  {
    minera::StreamingDictionaryOptions options;
    options.variant = minera::StreamingDictionaryVariant::kThreadSafe;
    minera::StreamingDictionary dictionary(options);
    std::vector<std::thread> writers;
    for (int worker = 0; worker < 4; ++worker) {
      writers.emplace_back([&dictionary, worker] {
        for (int item = 0; item < 50; ++item)
          dictionary.set("worker-" + std::to_string(worker) + "-" +
                             std::to_string(item), "value");
      });
    }
    for (auto& writer : writers) writer.join();
    assert(dictionary.size() == 200);
    assert(dictionary.contains("worker-3-49"));
    dictionary.flush_database();
    assert(dictionary.size() == 0);
  }

  {
    minera::StreamingDictionaryOptions options;
    options.variant = minera::StreamingDictionaryVariant::kRedis;
    options.port = 1;
    options.timeout_ms = 50;
    minera::StreamingDictionary dictionary(options);
    assert(!dictionary.available());
    bool rejected = false;
    try {
      minera::StreamingDictionaryOptions invalid = options;
      invalid.timeout_ms = 0;
      minera::StreamingDictionary bad(invalid);
      (void)bad;
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  std::filesystem::remove(path, ignored);
  return 0;
}
