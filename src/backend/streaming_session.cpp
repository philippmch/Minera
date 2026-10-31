#include "minera/streaming_session.hpp"
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <utility>

namespace minera {
namespace {
std::string quote(std::string_view value) {
  std::ostringstream out; out << '"';
  for (const unsigned char c : value) {
    if (c == '"') out << "\\\"";
    else if (c == '\\') out << "\\\\";
    else if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
    else out << char(c);
  }
  out << '"'; return out.str();
}
std::string method_name(StreamingSessionMethod method) {
  switch (method) {
    case StreamingSessionMethod::kDfg: return "dfg";
    case StreamingSessionMethod::kFootprints: return "footprints";
    case StreamingSessionMethod::kTemporal: return "temporal";
    case StreamingSessionMethod::kTbr: return "tbr";
    case StreamingSessionMethod::kDeclare: return "declare";
  }
  throw std::invalid_argument("unknown live session method");
}
}
struct StreamingSession::Impl {
  struct Delivery { std::size_t position, source_position; std::string batch, case_id, activity; bool accepted = false; std::size_t event_index = 0; std::string error; std::string native_event; };
  struct Batch { std::string id; std::size_t first, count; };
  explicit Impl(StreamingSessionOptions value) : options(std::move(value)), stream(1) {
    if (options.event_order != "input" && options.event_order != "imported") throw std::invalid_argument("unknown live event order");
    switch (options.method) {
      case StreamingSessionMethod::kDfg: dfg = std::make_unique<StreamingDfgDiscovery>(options.dfg); break;
      case StreamingSessionMethod::kFootprints:
        if (!options.footprint_model) throw std::invalid_argument("live footprints requires a model");
        footprints = std::make_unique<StreamingFootprintsConformance>(std::move(*options.footprint_model), options.footprints); break;
      case StreamingSessionMethod::kTemporal:
        if (!options.temporal_profile) throw std::invalid_argument("live temporal requires a profile");
        temporal = std::make_unique<StreamingTemporalConformance>(std::move(*options.temporal_profile), options.temporal); break;
      case StreamingSessionMethod::kTbr:
        if (!options.net) throw std::invalid_argument("live replay requires a net");
        tbr = std::make_unique<StreamingTokenReplay>(std::move(*options.net), options.tbr); break;
      case StreamingSessionMethod::kDeclare:
        if (!options.declare_model) throw std::invalid_argument("live Declare requires a model");
        declare = std::make_unique<DeclareStreamingConformance>(*options.declare_model); break;
    }
    register_monitor();
  }
  ~Impl() { stream.stop(); }
  void check_revision(std::size_t expected) const {
    if (expected != revision) throw StreamingSessionConflict("live session revision changed; refresh before mutating");
  }
  void editable() const {
    if (stream.state() == StreamingStreamState::kFinished) throw std::logic_error("the live stream is finished");
  }
  void drain_if_active() { if (stream.state() == StreamingStreamState::kActive) stream.drain(); }
  void register_monitor() {
    if (observer) return;
    observer = stream.register_observer([this](const Event& event) {
      Delivery delivery;
      { std::lock_guard lock(data_mutex); delivery = inputs.at(event.input_order); }
      try {
        // The reference DFG, footprints, temporal and TBR monitors skip an
        // event without a case id (footprints and TBR a None one too);
        // Declare files it under "undefined_case" itself. The monitors' own
        // Event overloads take the caller's case id, which may be a present
        // empty one.
        const auto kind = dfg ? StreamingMonitorKind::kDfg
                        : footprints ? StreamingMonitorKind::kFootprints
                        : temporal ? StreamingMonitorKind::kTemporal
                                   : StreamingMonitorKind::kTbr;
        if (!declare && !streaming_monitor_takes_event(event, kind)) {}
        else if (dfg) { delivery.accepted = dfg->receive(event, event.case_id); delivery.event_index = dfg->snapshot().events; }
        else if (footprints) { const auto r = footprints->receive(event, event.case_id); delivery.accepted = r.event_index != 0; delivery.event_index = r.event_index; }
        else if (temporal) { const auto r = temporal->receive(event, event.case_id); delivery.accepted = r.event_index != 0; delivery.event_index = r.event_index; }
        else if (declare) { const auto r = declare->receive(event); delivery.accepted = true; delivery.event_index = r.event_index; delivery.native_event = serialize_declare_stream_event_json(r); }
        else { const auto r = tbr->receive(event, event.case_id); delivery.accepted = r.accepted; delivery.event_index = r.event_index; }
      } catch (const std::exception& e) { delivery.error = e.what(); }
        catch (...) { delivery.error = "unknown native observer error"; }
      std::lock_guard lock(data_mutex); deliveries.push_back(std::move(delivery));
    });
  }
  std::string monitor_json() const {
    if (dfg) return serialize_streaming_dfg_json(dfg->snapshot());
    if (footprints) return serialize_streaming_footprints_json(footprints->snapshot());
    if (temporal) return serialize_streaming_temporal_json(temporal->snapshot());
    if (declare) return serialize_declare_streaming_json(declare->snapshot(),*options.declare_model);
    return serialize_streaming_tbr_json(tbr->snapshot());
  }
  void terminate(const std::string& id) {
    std::ostringstream out;
    out << "{\"case_id\":" << quote(id) << ",\"revision\":" << revision + 1;
    if (footprints) {
      const auto before = footprints->snapshot();
      if (!before.cases.contains(id)) throw std::invalid_argument("the case is not open");
      out << ",\"is_fit\":" << (footprints->terminate_case(id) ? "true" : "false");
    } else if (tbr) {
      const auto r = tbr->terminate_case(id);
      if (!r) throw std::invalid_argument("the case is not open");
      out << ",\"is_fit\":" << (r->is_fit ? "true" : "false") << ",\"missing\":" << r->missing << ",\"remaining\":" << r->remaining << ",\"marking\":[";
      for (std::size_t i=0;i<r->marking.size();++i) { if(i)out<<',';out<<r->marking[i]; } out << ']';
    } else throw std::invalid_argument("this monitor does not terminate cases");
    out << '}'; terminations.push_back(out.str());
  }
  StreamingSessionOptions options;
  mutable std::mutex operation_mutex;
  std::mutex data_mutex;
  std::size_t revision = 0;
  std::optional<std::size_t> observer;
  std::vector<Delivery> inputs, deliveries;
  std::vector<Batch> batches;
  std::map<std::string,std::size_t> batch_ids;
  std::vector<std::string> terminations;
  std::unique_ptr<StreamingDfgDiscovery> dfg;
  std::unique_ptr<StreamingFootprintsConformance> footprints;
  std::unique_ptr<StreamingTemporalConformance> temporal;
  std::unique_ptr<StreamingTokenReplay> tbr;
  std::unique_ptr<DeclareStreamingConformance> declare;
  LiveEventStream stream;
};
StreamingSession::StreamingSession(StreamingSessionOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
StreamingSession::~StreamingSession() = default;
std::size_t StreamingSession::revision() const { std::lock_guard lock(impl_->operation_mutex); return impl_->revision; }
std::size_t StreamingSession::append(std::vector<Event> events,const std::string& batch_id,std::size_t expected_revision) {
  auto& s=*impl_;std::lock_guard lock(s.operation_mutex);s.check_revision(expected_revision);s.editable();
  if(batch_id.empty())throw std::invalid_argument("live batch ID is empty");
  if(s.batch_ids.contains(batch_id))throw StreamingSessionConflict("this live batch ID has already been appended");
  std::size_t first;
  { std::lock_guard data_lock(s.data_mutex); first=s.inputs.size();
    for(std::size_t i=0;i<events.size();++i) { auto& e=events[i];s.inputs.push_back({first+i,e.input_order,batch_id,e.case_id,e.activity});e.input_order=first+i; }
  }
  for(auto& event:events) if(!s.stream.append(std::move(event)))throw std::logic_error("the live stream closed during append");
  s.batch_ids.emplace(batch_id,s.batches.size());s.batches.push_back({batch_id,first,events.size()});++s.revision;return events.size();
}
void StreamingSession::action(StreamingSessionAction action,std::size_t expected_revision,const std::string& case_id) {
  auto& s=*impl_;std::lock_guard lock(s.operation_mutex);s.check_revision(expected_revision);
  if(action==StreamingSessionAction::kDrain) { s.stream.drain();return; }
  if(action==StreamingSessionAction::kStart) { s.editable();s.stream.start();s.stream.drain(); }
  else if(action==StreamingSessionAction::kFinish) {
    if(s.stream.state()==StreamingStreamState::kInactive)throw std::logic_error("start the live stream before finishing");
    s.stream.stop();
  } else {
    s.drain_if_active();
    if(action==StreamingSessionAction::kRegister)s.register_monitor();
    else if(action==StreamingSessionAction::kDeregister) {if(s.observer)s.stream.deregister_observer(*s.observer);s.observer.reset();}
    else if(action==StreamingSessionAction::kResetAll) {
      if(s.dfg)s.dfg->reset();else if(s.footprints)s.footprints->reset_all();else if(s.temporal)s.temporal->reset_all();else if(s.declare)s.declare->reset_all();else s.tbr->reset_all();
    } else if(action==StreamingSessionAction::kResetCase) {
      if(case_id.empty())throw std::invalid_argument("live case ID is empty");
      if(s.dfg)throw std::invalid_argument("DFG supports resetting the complete monitor");
      if(s.footprints)s.footprints->reset_case(case_id);else if(s.temporal)s.temporal->reset_case(case_id);else if(s.declare)s.declare->reset_case(case_id);else s.tbr->reset_case(case_id);
    } else if(action==StreamingSessionAction::kTerminateCase)s.terminate(case_id);
    else if(action==StreamingSessionAction::kTerminateAll) {
      if(s.footprints) {for(const auto& [id,c]:s.footprints->snapshot().cases){(void)c;s.terminate(id);}}
      else if(s.tbr) {for(const auto& [id,c]:s.tbr->snapshot().cases){(void)c;s.terminate(id);}}
      else throw std::invalid_argument("this monitor does not terminate cases");
    } else throw std::invalid_argument("unknown live session action");
  }
  ++s.revision;
}
std::string StreamingSession::snapshot_json(bool wait_until_drained) {
  auto& s=*impl_;std::lock_guard lock(s.operation_mutex);if(wait_until_drained)s.drain_if_active();
  auto result=s.monitor_json();std::lock_guard data_lock(s.data_mutex);
  result.pop_back();result+=",\"input\":{\"event_order\":"+quote(s.options.event_order)+",\"parsed_events\":"+std::to_string(s.inputs.size())+"}}";
  const auto state=s.stream.state();std::ostringstream out;
  out<<"{\"family\":\"LIVE_STREAM_SESSION\",\"method\":"<<quote(method_name(s.options.method))<<",\"state\":"<<quote(state==StreamingStreamState::kInactive?"INACTIVE":state==StreamingStreamState::kActive?"ACTIVE":"FINISHED")<<",\"revision\":"<<s.revision<<",\"pending\":"<<s.stream.pending()<<",\"enqueued\":"<<s.inputs.size()<<",\"delivered\":"<<s.deliveries.size()<<",\"registered\":"<<(s.observer?"true":"false")<<",\"observer_id\":"<<(s.observer?std::to_string(*s.observer):"null")<<",\"result\":"<<result<<",\"batches\":[";
  for(std::size_t i=0;i<s.batches.size();++i) {const auto& b=s.batches[i];if(i)out<<',';out<<"{\"batch_id\":"<<quote(b.id)<<",\"first_position\":"<<b.first<<",\"events\":"<<b.count<<'}';}out<<"],\"deliveries\":[";
  for(std::size_t i=0;i<s.deliveries.size();++i) {const auto& d=s.deliveries[i];if(i)out<<',';out<<"{\"position\":"<<d.position<<",\"source_position\":"<<d.source_position<<",\"batch_id\":"<<quote(d.batch)<<",\"case_id\":"<<quote(d.case_id)<<",\"activity\":"<<quote(d.activity)<<",\"accepted\":"<<(d.accepted?"true":"false")<<",\"event_index\":"<<d.event_index<<",\"error\":"<<(d.error.empty()?"null":quote(d.error));if(!d.native_event.empty())out<<",\"native_event\":"<<d.native_event;out<<'}';}out<<"],\"terminations\":[";
  for(std::size_t i=0;i<s.terminations.size();++i){if(i)out<<',';out<<s.terminations[i];}out<<"]}";return out.str();
}
}  // namespace minera
