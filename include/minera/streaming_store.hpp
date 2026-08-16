#pragma once

#include <filesystem>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/streaming_dfg.hpp"

namespace minera {

// Dictionary backends used by streaming algorithms. The remote backend speaks
// the small command subset needed by the dictionary API directly, so no client
// library is required by callers.
enum class StreamingDictionaryVariant {
  kClassic,
  kThreadSafe,
  kRedis,
  CLASSIC = kClassic,
  THREAD_SAFE = kThreadSafe,
  REDIS = kRedis,
};

struct StreamingDictionaryOptions {
  StreamingDictionaryVariant variant =
      StreamingDictionaryVariant::kThreadSafe;
  std::string hostname = "127.0.0.1";
  std::uint16_t port = 6379;
  std::size_t database = 0;
  std::string key_prefix;
  std::uint32_t timeout_ms = 500;
};

// A string dictionary with the classic, mutex-protected, and remote store
// variants used by streaming utilities. Local variants keep deterministic
// lexicographic key order; the remote variant preserves the same view while
// isolating keys with the configured prefix.
class StreamingDictionary {
 public:
  explicit StreamingDictionary(StreamingDictionaryOptions options = {});
  ~StreamingDictionary();

  StreamingDictionary(const StreamingDictionary&) = delete;
  StreamingDictionary& operator=(const StreamingDictionary&) = delete;
  StreamingDictionary(StreamingDictionary&&) noexcept;
  StreamingDictionary& operator=(StreamingDictionary&&) noexcept;

  [[nodiscard]] StreamingDictionaryVariant variant() const noexcept;
  [[nodiscard]] const StreamingDictionaryOptions& options() const noexcept;

  void set(std::string_view key, std::string_view value);
  [[nodiscard]] std::optional<std::string> get(std::string_view key) const;
  [[nodiscard]] bool contains(std::string_view key) const;
  bool erase(std::string_view key);
  [[nodiscard]] std::vector<std::string> keys() const;
  [[nodiscard]] std::vector<std::string> values() const;
  [[nodiscard]] std::size_t size() const;
  void clear();

  // For local stores these are aliases for clear(). For the remote backend,
  // flush_database removes the selected database and flush_all removes every
  // database, matching the corresponding server commands.
  void flush_database();
  void flush_all();

  // Checks a remote endpoint without changing data. Local stores are always
  // available.
  [[nodiscard]] bool available() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

using StreamingDictio = StreamingDictionary;
using StreamingDictioOptions = StreamingDictionaryOptions;

[[nodiscard]] inline StreamingDictionary make_streaming_dictionary(
    StreamingDictionaryOptions options = {}) {
  return StreamingDictionary(std::move(options));
}

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
