#include "minera/streaming_store.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  const auto path = std::filesystem::temp_directory_path() /
                    "minera_streaming_dfg_store_test.csv";
  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  {
    minera::PersistentStreamingDfg store(path);
    assert(!store.receive("", "A"));
    assert(store.receive("case,1", "A"));
    assert(store.receive("case,1", "B"));
    assert(store.receive("case\"2", "A\nstart"));
    const auto snapshot = store.snapshot();
    assert(snapshot.events == 3);
    assert(snapshot.graph.at({"A", "B"}) == 1);
    assert(snapshot.start_activities.at("A") == 1);
    assert(snapshot.open_cases.at("case,1") == "B");
    assert(snapshot.open_cases.at("case\"2") == "A\nstart");
  }

  {
    minera::PersistentStreamingDfg reopened(path);
    const auto snapshot = reopened.snapshot();
    assert(snapshot.events == 3);
    assert(snapshot.graph.at({"A", "B"}) == 1);
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

  std::filesystem::remove(path, ignored);
  return 0;
}
