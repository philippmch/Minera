#pragma once

// Admission control for POST /api/mine: one mine at a time, and a size cap
// checked on the upload's bytes before anything is parsed.
//
// A mine peaks at about 8 KiB per event for a log with case, activity,
// timestamp and resource columns (measured on a 200,000-event CSV: 1.59 GB
// peak, 0.88 GB retained); the estimate uses 10 KiB per event for headroom,
// and 2 KiB more per event for every further column. The cap
// refuses a mine whose estimate passes the memory budget (half the physical
// memory, or the cgroup limit when lower, unless --mine-memory-budget sets it)
// or whose event count passes --max-mine-events, with 413 MINE_LIMIT, rather
// than letting the server run out of memory part-way.

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <atomic>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

constexpr std::uint64_t kMineBytesPerEvent = 10 * 1024;
constexpr std::uint64_t kMineBytesPerExtraColumn = 2 * 1024;
// Columns the per-event figure already includes.
constexpr std::uint64_t kMineBaseColumns = 4;

// 0 means "half of the physical memory" (resolved once, on first use).
std::uint64_t g_mine_memory_budget = 0;
// 0 means no event-count cap besides the memory budget.
std::uint64_t g_max_mine_events = 0;

std::uint64_t physical_memory_bytes() {
  std::uint64_t total = 0;
#if defined(_WIN32)
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof status;
  if (GlobalMemoryStatusEx(&status)) total = status.ullTotalPhys;
#else
  const long pages = ::sysconf(_SC_PHYS_PAGES);
  const long page = ::sysconf(_SC_PAGESIZE);
  if (pages > 0 && page > 0) total = static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(page);
#endif
#if defined(__linux__)
  // A container's memory limit is the memory the server really has.
  for (const char* path : {"/sys/fs/cgroup/memory.max", "/sys/fs/cgroup/memory/memory.limit_in_bytes"}) {
    std::ifstream file(path);
    std::uint64_t limit = 0;
    if (file >> limit && limit > 0 && (total == 0 || limit < total)) total = limit;
  }
#endif
  return total;
}

std::uint64_t mine_memory_budget() {
  static const std::uint64_t half = [] {
    const auto total = physical_memory_bytes();
    // Unknown memory: 4 GiB, a typical laptop's half.
    return total == 0 ? std::uint64_t{4} << 30 : total / 2;
  }();
  return g_mine_memory_budget != 0 ? g_mine_memory_budget : half;
}

struct MineSizeEstimate {
  bool known = false;  // false: the input was not pre-scanned (no check)
  std::uint64_t events = 0;
  std::uint64_t columns = 0;
  std::uint64_t extra_columns() const {return columns > kMineBaseColumns ? columns - kMineBaseColumns : 0;}
  std::uint64_t bytes_for(std::uint64_t retained_events) const {
    const auto per_event = kMineBytesPerEvent + kMineBytesPerExtraColumn * extra_columns();
    if (retained_events != 0 && per_event > UINT64_MAX / retained_events) return UINT64_MAX;
    return retained_events * per_event;
  }
};

// Quote-aware CSV scan: data rows (non-blank records after the header) and
// the header's columns. Quotes follow the importer (read_csv_record): a quote
// opens a quoted field only as a field's first character, "" inside one is a
// literal quote, and a quote elsewhere in an unquoted field is literal text;
// CR, LF and CRLF end a record outside quotes. Row shapes follow the
// importer's read_csv rules without being checked here: fields a first data
// row has beyond the header are an implicit index, not frame columns, so the
// columns are the header's; short rows and rows the importer refuses for
// having too many fields still count as rows (the import then answers 400).
MineSizeEstimate scan_csv_mine_size(std::string_view bytes, char delimiter) {
  MineSizeEstimate estimate;
  estimate.known = true;
  std::size_t i = 0;
  if (bytes.size() >= 3 && bytes.substr(0, 3) == "\xEF\xBB\xBF") i = 3;
  enum class Quote { kFieldStart, kUnquoted, kQuoted, kQuoteInQuoted };
  auto state = Quote::kFieldStart;
  bool header_done = false, row_has_text = false, row_open = false;
  std::size_t field = 0;
  const auto end_row = [&] {
    ++field;
    if (row_has_text) {
      if (!header_done) {estimate.columns = field; header_done = true;}
      else ++estimate.events;
    }
    field = 0; row_has_text = false; row_open = false;
    state = Quote::kFieldStart;
  };
  for (; i < bytes.size(); ++i) {
    const char c = bytes[i];
    if (state == Quote::kQuoted) {
      if (c == '"') state = Quote::kQuoteInQuoted;
      continue;
    }
    if (state == Quote::kQuoteInQuoted && c == '"') {state = Quote::kQuoted; continue;}
    if (c == '\r' || c == '\n') {
      if (c == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i;
      end_row();
      continue;
    }
    row_open = true;
    if (c == delimiter) {++field; row_has_text = true; state = Quote::kFieldStart; continue;}
    if (state == Quote::kFieldStart && c == '"') {state = Quote::kQuoted; row_has_text = true; continue;}
    state = Quote::kUnquoted;
    if (c != ' ' && c != '\t') row_has_text = true;
  }
  if (row_open || state != Quote::kFieldStart) end_row();
  return estimate;
}

// XES: <event> elements and the attribute keys of the first trace and its
// first event (each a column of the flattened log). With xes_max_traces the
// importer stops after that many traces, so events past them are not counted
// (as reader_estimated_events does).
class XesMineScanner {
 public:
  explicit XesMineScanner(std::size_t max_traces = std::numeric_limits<std::size_t>::max())
      : max_traces_(max_traces) {}
  // True once a trace past max_traces has started: nothing later is imported.
  bool full() const {return full_;}
  void feed(std::string_view chunk) {
    if (full_) return;
    // A tag cut by the chunk's end is carried into the next one.
    std::string text = std::move(carry_);
    carry_.clear();
    text.append(chunk);
    std::size_t at = 0, pending = std::string::npos;
    while ((at = text.find('<', at)) != std::string::npos) {
      const auto tag = std::string_view(text).substr(at);
      const bool in_first = state_ == kInFirstTrace || state_ == kInFirstEvent;
      // Inside the first trace a whole tag is needed to see its key.
      if (tag.size() < 8 || (in_first && tag.find('>') == std::string_view::npos)) {pending = at; break;}
      const auto boundary = [&](std::size_t n) {
        const char c = tag[n];
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '>' || c == '/';
      };
      if (tag.starts_with("<event") && boundary(6)) {
        ++events_;
        if (state_ == kInFirstTrace) state_ = kInFirstEvent;
      } else if (tag.starts_with("<trace") && boundary(6)) {
        if (++traces_ > max_traces_) {full_ = true; carry_.clear(); return;}
        if (state_ == kBeforeTrace) state_ = kInFirstTrace;
      } else if (tag.starts_with("</event") && state_ == kInFirstEvent) {
        state_ = kDone;
      } else if (in_first && tag[1] != '/') {
        const auto key = tag.find("key=");
        if (key != std::string_view::npos && key < tag.find('>')) ++keys_;
      }
      ++at;
    }
    // A malformed tag with no end in sight is dropped, not carried forever.
    if (pending != std::string::npos && text.size() - pending <= 65536) carry_ = text.substr(pending);
  }
  MineSizeEstimate result() const {
    MineSizeEstimate estimate;
    estimate.known = true;
    estimate.events = events_;
    estimate.columns = keys_;
    return estimate;
  }

 private:
  enum State {kBeforeTrace, kInFirstTrace, kInFirstEvent, kDone};
  std::string carry_;
  std::uint64_t events_ = 0, keys_ = 0;
  std::size_t traces_ = 0, max_traces_;
  bool full_ = false;
  State state_ = kBeforeTrace;
};

// A gzip XES is counted as it inflates, up to the request's decompressed
// bound; past it (or when corrupt) the importer refuses it with its own error.
// The scan stops at the first trace past max_traces.
MineSizeEstimate scan_xes_mine_size(std::string_view bytes, std::size_t max_decompressed_bytes,
                                    std::size_t max_traces = std::numeric_limits<std::size_t>::max()) {
  XesMineScanner scanner(max_traces);
  if (minera::detail::is_gzip_data(bytes)) {
#if defined(CPPHTTPLIB_ZLIB_SUPPORT)
    httplib::detail::gzip_decompressor inflater;
    if (!inflater.is_valid()) return {};
    constexpr std::size_t kSlice = 1 << 20;
    std::size_t inflated = 0;
    for (std::size_t offset = 0; offset < bytes.size() && !scanner.full(); offset += kSlice) {
      const auto length = std::min(kSlice, bytes.size() - offset);
      if (!inflater.decompress(bytes.data() + offset, length, [&](const char* data, std::size_t size) {
            inflated += size;
            if (inflated > max_decompressed_bytes) return false;
            scanner.feed(std::string_view(data, size));
            return !scanner.full();
          }) && !scanner.full())
        return {};
    }
#else
    (void)max_decompressed_bytes;
    return {};  // Without zlib a gzip XES is not pre-scanned.
#endif
  } else {
    constexpr std::size_t kSlice = 1 << 20;
    for (std::size_t offset = 0; offset < bytes.size() && !scanner.full(); offset += kSlice)
      scanner.feed(bytes.substr(offset, std::min(kSlice, bytes.size() - offset)));
  }
  return scanner.result();
}

// Thrown when a mine would pass the size cap; carries the 413 body.
struct MineLimitError : std::runtime_error {
  std::string body;
  MineLimitError(const std::string& message, std::string json)
      : std::runtime_error(message), body(std::move(json)) {}
};

// The whole upload is imported and kept as the source (a sampling or filter
// control trims only the mined view), so the estimate is for every event.
void check_mine_size_limit(const httplib::Request& request, const std::string& bytes,
                           const std::string& dataset, const minera::MineParams& params) {
  if (active_source_field(request, "mining_input_format") == "table") return;  // JSON tables are not pre-scanned
  const bool xes = looks_like_xes(dataset, bytes);
  const auto estimate = xes ? scan_xes_mine_size(bytes, params.xes.max_decompressed_bytes, params.xes.max_traces)
                            : scan_csv_mine_size(bytes, params.csv.delimiter);
  if (!estimate.known) return;
  const auto budget = mine_memory_budget();
  const auto needed = estimate.bytes_for(estimate.events);
  const bool over_events = g_max_mine_events != 0 && estimate.events > g_max_mine_events;
  const bool over_budget = needed > budget;
  if (!over_events && !over_budget) return;
  // The memory budget is the one that protects the server, so it is named
  // when both are passed.
  const std::string limit = over_budget ? "mine_memory_budget" : "max_mine_events";
  const auto mib = [](std::uint64_t value) {return std::to_string((value + (1u << 20) - 1) >> 20) + " MiB";};
  const std::string message = over_budget
      ? "mining this log would need about " + mib(needed) + " (" + std::to_string(estimate.events) +
            " events), more than the server's mining memory budget of " + mib(budget)
      : "the log has about " + std::to_string(estimate.events) + " events, more than the server's limit of " +
            std::to_string(g_max_mine_events) + " (--max-mine-events)";
  const std::string hint =
      "Filter or split the log before uploading it (fewer cases or a shorter period), or start "
      "minera_server with a larger --mine-memory-budget or --max-mine-events.";
  std::string json = "{\"family\":\"MINE_LIMIT\",\"error\":" +
      minera::embedding_runtime::detail::escape_json(message + ". " + hint) +
      ",\"limit\":\"" + limit + "\",\"estimated_events\":" + std::to_string(estimate.events) +
      ",\"estimated_bytes\":" + std::to_string(needed) +
      ",\"extra_columns\":" + std::to_string(estimate.extra_columns()) +
      ",\"memory_budget_bytes\":" + std::to_string(budget) +
      ",\"max_mine_events\":" + (g_max_mine_events != 0 ? std::to_string(g_max_mine_events) : std::string("null")) +
      ",\"hint\":" + minera::embedding_runtime::detail::escape_json(hint) + '}';
  throw MineLimitError(message, std::move(json));
}

// One /api/mine runs at a time: mining a large log holds gigabytes, and two at
// once would double that. A request that finds the slot taken gets 503 with
// Retry-After. "canceling" tells a client that just canceled (or replaced)
// the running mine that the slot frees at its next stage boundary.
std::atomic<bool> g_mine_running = false;

class MineSlot {
 public:
  MineSlot() : held_(!g_mine_running.exchange(true)) {}
  ~MineSlot() {
    if (!held_) return;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      g_running_mine_ticket.reset();
    }
    g_mine_running.store(false);
  }
  MineSlot(const MineSlot&) = delete;
  MineSlot& operator=(const MineSlot&) = delete;
  bool held() const {return held_;}

 private:
  bool held_;
};

void answer_mine_busy(httplib::Response& response) {
  bool canceling = false;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    if (const auto running = g_running_mine_ticket.lock())
      canceling = running->canceled.load() || running->generation != g_active_mining_generation.load();
  }
  const int retry = canceling ? 1 : 5;
  response.status = 503;
  response.set_header("Retry-After", std::to_string(retry));
  response.set_content(std::string("{\"family\":\"MINE_BUSY\",\"error\":\"Another log is being mined \\u2014 wait or cancel it.\"") +
      ",\"canceling\":" + (canceling ? "true" : "false") + ",\"retry_after_seconds\":" + std::to_string(retry) + '}',
      "application/json");
}
