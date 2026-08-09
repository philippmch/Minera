#include "minera/live_stream.hpp"

#include <atomic>
#include <cassert>
#include <stdexcept>

namespace {

void event_stream_drains_and_finishes() {
  minera::LiveEventStream stream(2);
  std::atomic<std::size_t> seen = 0;
  const auto observer = stream.register_observer(
      [&](const minera::Event& event) {
        if (!event.activity.empty()) ++seen;
      });
  assert(stream.state() == minera::StreamingStreamState::kInactive);
  assert(stream.append({"c", "A", "", "", 0, {}, ""}));
  stream.start();
  assert(stream.state() == minera::StreamingStreamState::kActive);
  assert(stream.append({"c", "B", "", "", 1, {}, ""}));
  stream.stop();
  assert(seen == 2);
  assert(stream.state() == minera::StreamingStreamState::kFinished);
  assert(!stream.append({"c", "C", "", "", 2, {}, ""}));
  assert(stream.deregister_observer(observer));
  assert(!stream.deregister_observer(observer));
}

void trace_stream_and_failures_are_isolated() {
  minera::LiveTraceStream stream(1);
  std::atomic<std::size_t> events = 0;
  const auto observer = stream.register_observer(
      [&](const minera::Trace& trace) {
        events += trace.events.size();
        throw std::runtime_error("observer failure");
      });
  minera::Trace trace;
  trace.case_id = "c";
  trace.events.push_back({"c", "A", "", "", 0, {}, ""});
  assert(stream.append(trace));
  stream.start();
  stream.stop();
  assert(events == 1);
  assert(stream.deregister_observer(observer));
}

void invalid_pool_and_restart_are_rejected() {
  bool rejected = false;
  try {
    minera::LiveEventStream invalid(0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  minera::LiveEventStream stream(1);
  stream.start();
  stream.stop();
  bool restart_rejected = false;
  try {
    stream.start();
  } catch (const std::logic_error&) {
    restart_rejected = true;
  }
  assert(restart_rejected);
}

}  // namespace

int main() {
  event_stream_drains_and_finishes();
  trace_stream_and_failures_are_isolated();
  invalid_pool_and_restart_are_rejected();
  return 0;
}
