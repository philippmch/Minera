#pragma once

// Progress of the running POST /api/mine, for GET /api/mine-progress.
//
// The mine that holds the /api/mine slot (mine_limits.hpp) records which
// stage it is in. The record has its own mutex, never g_session_mutex or the
// mine slot, so a poll from another connection answers at once while the mine
// runs (and while another request is refused with MINE_BUSY). A poll reads
// nothing of the log: stage names are fixed text.
//
// mining_cancel_hook is the cancellation callback the long stages poll: a
// client disconnect, as before, and also a cancel-mine (or a newer source
// request) for this mine, so a cancel stops the work inside a stage instead
// of at the next stage boundary.

#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

struct MineProgressRecord {
  bool running = false;
  std::string request_id;  // the mine's mining_request_id ("" when it sent none)
  std::string stage;
  std::size_t stage_index = 0;  // 1-based index of the current stage
  std::size_t stages_total = 0;  // 0 when the stage count is not known up front
  std::chrono::steady_clock::time_point started{}, stage_started{};
};

std::mutex g_mine_progress_mutex;
MineProgressRecord g_mine_progress;  // under g_mine_progress_mutex

// Held by the running mine: records its stages and clears the record when the
// mine ends, however it ends.
class MineProgress {
 public:
  MineProgress(std::string request_id, std::size_t stages_total, std::string_view first_stage) {
    std::lock_guard<std::mutex> lock(g_mine_progress_mutex);
    const auto now = std::chrono::steady_clock::now();
    g_mine_progress = MineProgressRecord{true, std::move(request_id), std::string(first_stage), 1,
                                         stages_total, now, now};
  }
  ~MineProgress() {
    std::lock_guard<std::mutex> lock(g_mine_progress_mutex);
    g_mine_progress = MineProgressRecord{};
  }
  MineProgress(const MineProgress&) = delete;
  MineProgress& operator=(const MineProgress&) = delete;

};

// The running mine has finished its current stage and begins `name`; nothing
// when no mine is recorded.
void mine_progress_stage(std::string_view name) {
  std::lock_guard<std::mutex> lock(g_mine_progress_mutex);
  if (!g_mine_progress.running) return;
  g_mine_progress.stage = std::string(name);
  ++g_mine_progress.stage_index;
  if (g_mine_progress.stages_total != 0 && g_mine_progress.stage_index > g_mine_progress.stages_total)
    g_mine_progress.stages_total = g_mine_progress.stage_index;
  g_mine_progress.stage_started = std::chrono::steady_clock::now();
}

// The running mine's stage count: 0 when it is not known up front.
void mine_progress_total(std::size_t stages_total) {
  std::lock_guard<std::mutex> lock(g_mine_progress_mutex);
  if (g_mine_progress.running) g_mine_progress.stages_total = stages_total;
}

std::function<bool()> mining_cancel_hook(const std::shared_ptr<ActiveMiningTicket>& ticket,
                                         std::function<bool()> disconnected) {
  return [ticket, disconnected = std::move(disconnected)] {
    return ticket->canceled.load() || ticket->generation != g_active_mining_generation.load() ||
           (disconnected && disconnected());
  };
}

// Whether a mine that stopped with an exception stopped because it was
// canceled or replaced (rather than for a client disconnect or an error).
bool mining_ticket_canceled(const std::shared_ptr<ActiveMiningTicket>& ticket) {
  return ticket && (ticket->canceled.load() || ticket->generation != g_active_mining_generation.load());
}

// GET /api/mine-progress[?request_id=UUID]
//   {"family":"MINE_PROGRESS","running":false}
//   {"family":"MINE_PROGRESS","running":true,"request_matches":true,
//    "stage":"Inductive net: reachability","stage_index":13,"stages_done":12,
//    "stages_total":26,"elapsed_seconds":12.5,"stage_elapsed_seconds":10.2,
//    "canceling":false}
// request_matches is present only when request_id is given: whether the
// running mine is that request. stages_total is null when the mine's stage
// count is not known up front (the native-source dispatch). canceling: the
// running mine was canceled or replaced and is stopping.
void handle_mine_progress(const httplib::Request& request, httplib::Response& response) {
  for (const auto& [name, value] : request.params) {
    (void)value;
    if (name != "request_id") {
      response.status = 400;
      response.set_content(error_json("mine-progress takes only request_id"), "application/json");
      return;
    }
  }
  MineProgressRecord record;
  {
    std::lock_guard<std::mutex> lock(g_mine_progress_mutex);
    record = g_mine_progress;
  }
  response.set_header("Cache-Control", "no-store");
  if (!record.running) {
    response.set_content("{\"family\":\"MINE_PROGRESS\",\"running\":false}", "application/json");
    return;
  }
  bool canceling = false;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    if (const auto running = g_running_mine_ticket.lock())
      canceling = running->canceled.load() || running->generation != g_active_mining_generation.load();
  }
  const auto now = std::chrono::steady_clock::now();
  const auto seconds = [&](std::chrono::steady_clock::time_point since) {
    char text[32];
    std::snprintf(text, sizeof text, "%.3f", std::chrono::duration<double>(now - since).count());
    return std::string(text);
  };
  std::string body = "{\"family\":\"MINE_PROGRESS\",\"running\":true";
  if (request.has_param("request_id")) {
    const bool matches = !record.request_id.empty() && request.get_param_value("request_id") == record.request_id;
    body += std::string(",\"request_matches\":") + (matches ? "true" : "false");
  }
  body += ",\"stage\":" + minera::embedding_runtime::detail::escape_json(record.stage) +
          ",\"stage_index\":" + std::to_string(record.stage_index) +
          ",\"stages_done\":" + std::to_string(record.stage_index - 1) +
          ",\"stages_total\":" + (record.stages_total ? std::to_string(record.stages_total) : std::string("null")) +
          ",\"elapsed_seconds\":" + seconds(record.started) +
          ",\"stage_elapsed_seconds\":" + seconds(record.stage_started) +
          ",\"canceling\":" + (canceling ? "true" : "false") + '}';
  response.set_content(body, "application/json");
}
