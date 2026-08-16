#pragma once

#include <filesystem>
#include <mutex>
#include <string_view>

#include "minera/streaming_dfg.hpp"

namespace minera {

// A restartable, file-backed streaming DFG. The append-only state file keeps
// the accepted case/activity pairs in CSV form, so reopening the store
// reconstructs open cases and all counters without requiring a live stream.
// The file format is private to this class and is intentionally independent of
// caller-selected CSV column names.
class PersistentStreamingDfg {
 public:
  explicit PersistentStreamingDfg(std::filesystem::path path,
                                  StreamingDfgOptions options = {});

  PersistentStreamingDfg(const PersistentStreamingDfg&) = delete;
  PersistentStreamingDfg& operator=(const PersistentStreamingDfg&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }
  [[nodiscard]] const StreamingDfgOptions& options() const noexcept {
    return discovery_.options();
  }

  bool receive(std::string_view case_id, std::string_view activity);
  bool receive(const Event& event, std::string_view case_id);

  [[nodiscard]] StreamingDfgSnapshot snapshot() const;
  void reset();

 private:
  void append(std::string_view case_id, std::string_view activity);

  std::filesystem::path path_;
  StreamingDfgDiscovery discovery_;
  mutable std::mutex mutex_;
};

}  // namespace minera
