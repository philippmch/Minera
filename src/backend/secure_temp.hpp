#pragma once

// Private temporary directories for uploads and exports that must reach the
// file system (SQLite and CSV pairs).
//
// On POSIX the directory is created by mkdtemp: mode 0700, with a name
// another local user can neither predict nor pre-create. Each live directory
// is entered in a fixed-size, lock-free registry. A SIGTERM or SIGINT handler
// (install_signal_cleanup) uses only async-signal-safe calls (unlink and
// rmdir on paths stored in advance) to remove the known files in each live
// directory and the directory itself. It then re-raises the signal with the
// default action, so the process still ends as it did before.
//
// Header-only. The registry is an inline variable, so the core library and
// the server share one copy.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#endif

namespace minera::secure_temp {

// The only file names placed in a registered directory. The signal handler
// removes exactly these, because it cannot list a directory safely.
inline constexpr const char* kKnownFiles[] = {
    "original.data", "original.data-journal", "original.data-wal", "original.data-shm",
    "ocel.sqlite", "ocel.sqlite-journal", "ocel.sqlite-wal", "ocel.sqlite-shm",
    "events.csv", "objects.csv"};

namespace detail {
inline constexpr std::size_t kSlots = 64;
inline constexpr std::size_t kPathMax = 1024;
enum SlotState : int { kFree = 0, kBusy = 1, kReady = 2 };
struct Slot {
  std::atomic<int> state{kFree};
  char path[kPathMax] = {};
};
inline Slot g_slots[kSlots];

inline std::size_t append(char* out, std::size_t at, const char* text) noexcept {
  while (*text != '\0' && at + 1 < kPathMax) out[at++] = *text++;
  out[at] = '\0';
  return at;
}

#if !defined(_WIN32)
inline void remove_slot_files(const char* directory) noexcept {
  char file[kPathMax];
  for (const char* name : kKnownFiles) {
    std::size_t at = append(file, 0, directory);
    at = append(file, at, "/");
    append(file, at, name);
    ::unlink(file);
  }
  ::rmdir(directory);
}

inline void on_signal(int signal_number) noexcept {
  for (auto& slot : g_slots) {
    int expected = kReady;
    // Claim the slot so a thread unregistering it concurrently leaves it alone.
    if (slot.state.compare_exchange_strong(expected, kBusy, std::memory_order_acquire))
      remove_slot_files(slot.path);
  }
  // SA_RESETHAND restored the default action: end the process as before.
  ::raise(signal_number);
}
#endif
}  // namespace detail

// Removes the directory (recursively) when destroyed, and keeps it in the
// signal-cleanup registry while it exists.
class PrivateDirectory {
 public:
  explicit PrivateDirectory(std::string_view prefix) {
    const auto base = std::filesystem::temp_directory_path();
#if !defined(_WIN32)
    std::string pattern = (base / (std::string(prefix) + "XXXXXX")).string();
    if (::mkdtemp(pattern.data()) == nullptr)
      throw std::runtime_error("cannot create a private temporary directory");
    path_ = pattern;
    register_slot();
#else
    // random_device may be deterministic on some runtimes, so the name also
    // mixes the clock, the process id and a process-wide counter: two
    // processes, or two directories in one process, never draw the same name
    // even then. create_directory fails on an existing name, so a clash only
    // costs another attempt.
    static std::atomic<std::uint64_t> counter{0};
    std::random_device entropy;
    const auto pid = static_cast<std::uint64_t>(::_getpid());
    for (int attempt = 0; attempt < 64 && path_.empty(); ++attempt) {
      const auto now = static_cast<std::uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count());
      const std::uint64_t random =
          (static_cast<std::uint64_t>(entropy()) << 32) ^ static_cast<std::uint64_t>(entropy());
      const std::uint64_t mixed =
          random ^ (now * 0x9E3779B97F4A7C15ull) ^
          (counter.fetch_add(1, std::memory_order_relaxed) * 0xBF58476D1CE4E5B9ull);
      const auto candidate = base / (std::string(prefix) + std::to_string(pid) + "-" +
                                     std::to_string(mixed));
      std::error_code error;
      if (std::filesystem::create_directory(candidate, error)) path_ = candidate;
    }
    if (path_.empty()) throw std::runtime_error("cannot create a private temporary directory");
#endif
  }
  PrivateDirectory(const PrivateDirectory&) = delete;
  PrivateDirectory& operator=(const PrivateDirectory&) = delete;
  ~PrivateDirectory() { remove(); }

  const std::filesystem::path& path() const noexcept { return path_; }
  std::filesystem::path file(const char* name) const { return path_ / name; }

  void remove() noexcept {
    if (path_.empty()) return;
    unregister_slot();
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
    path_.clear();
  }

 private:
  void register_slot() noexcept {
    const auto text = path_.string();
    if (text.size() + 32 >= detail::kPathMax) return;
    for (std::size_t i = 0; i < detail::kSlots; ++i) {
      int expected = detail::kFree;
      if (!detail::g_slots[i].state.compare_exchange_strong(expected, detail::kBusy)) continue;
      std::memcpy(detail::g_slots[i].path, text.c_str(), text.size() + 1);
      detail::g_slots[i].state.store(detail::kReady, std::memory_order_release);
      slot_ = static_cast<int>(i);
      return;
    }
    // A full registry only skips signal-time cleanup; normal cleanup remains.
  }
  void unregister_slot() noexcept {
    if (slot_ < 0) return;
    int expected = detail::kReady;
    // If the signal handler claimed it, the process is ending anyway.
    if (detail::g_slots[slot_].state.compare_exchange_strong(expected, detail::kBusy)) {
      detail::g_slots[slot_].path[0] = '\0';
      detail::g_slots[slot_].state.store(detail::kFree, std::memory_order_release);
    }
    slot_ = -1;
  }

  std::filesystem::path path_;
  int slot_ = -1;
};

// Installs the SIGTERM/SIGINT cleanup once; signals with a handler already
// installed (or ignored) keep it.
inline void install_signal_cleanup() noexcept {
#if !defined(_WIN32)
  for (const int signal_number : {SIGTERM, SIGINT}) {
    struct sigaction current {};
    if (::sigaction(signal_number, nullptr, &current) != 0) continue;
    if (current.sa_handler != SIG_DFL) continue;
    struct sigaction action {};
    action.sa_handler = detail::on_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;
    ::sigaction(signal_number, &action, nullptr);
  }
#endif
}

}  // namespace minera::secure_temp
