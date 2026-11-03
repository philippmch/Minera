#include "minera/reader_stream_session.hpp"
#include "minera/log_conversion.hpp"
#include "minera/streaming_session.hpp"
#include <iomanip>
#include <mutex>
#include <set>
#include <map>
#include <tuple>
#include <sstream>
#include <utility>

namespace minera {
namespace {
std::string json_quote(std::string_view text) {
  std::ostringstream out;out << '"';
  for (unsigned char c : text) {
    if (c == '"') out << "\\\"";
    else if (c == '\\') out << "\\\\";
    else if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
    else out << char(c);
  }
  out << '"';return out.str();
}
Trace event_trace(const Event& event) {
  Trace trace;trace.case_id=event.case_id;trace.case_id_state=event.case_id_state;
  trace.case_id_source_key=event.case_id_source_key;trace.events.push_back(event);return trace;
}
std::string native_item(const Trace& trace, const EventLog& context) {
  LogConversionResult value;copy_event_log_context(context,value.log);
  value.log.traces.push_back(trace);return log_conversion_json(value);
}
}
struct ReaderStreamSession::Impl {
  struct Item { std::size_t position,generation,reader_position;std::string batch;Trace trace; };
  struct Batch { std::string id;std::size_t generation,reader_position,first,count; };
  explicit Impl(ReaderStreamOptions options) : kind(options.kind) {
    std::istringstream input(options.source);
    if(kind==ReaderStreamKind::kCsvEvents) {csv=std::make_unique<CsvEventStreamReader>(input,options.csv,options.events);copy_event_log_context(csv->source_context(),context);}
    else if(kind==ReaderStreamKind::kXesEvents) {xes=std::make_unique<XesEventStreamReader>(input,options.xes,options.events);copy_event_log_context(xes->source_context(),context);}
    else {traces=std::make_unique<XesTraceStreamReader>(input,options.xes,options.traces);copy_event_log_context(traces->source_context(),context);}
    if(traces)trace_stream=std::make_unique<LiveTraceStream>(1);else event_stream=std::make_unique<LiveEventStream>(1);
    register_observer();
  }
  ~Impl() { stop(); }
  StreamingStreamState state() const { return trace_stream?trace_stream->state():event_stream->state(); }
  std::size_t pending() const { return trace_stream?trace_stream->pending():event_stream->pending(); }
  void start() { if(trace_stream)trace_stream->start();else event_stream->start(); }
  void drain() { if(trace_stream)trace_stream->drain();else event_stream->drain(); }
  void stop() { if(trace_stream)trace_stream->stop();else if(event_stream)event_stream->stop(); }
  bool exhausted() const { return traces?traces->exhausted():csv?csv->exhausted():xes->exhausted(); }
  void register_observer() {
    if(observer)return;
    if(trace_stream)observer=trace_stream->register_observer([this](const Trace& trace){std::lock_guard lock(data_mutex);deliveries.push_back(trace);});
    else observer=event_stream->register_observer([this](const Event& event){std::lock_guard lock(data_mutex);deliveries.push_back(event_trace(event));});
  }
  void deregister_observer() {
    if(!observer)return;
    if(trace_stream)trace_stream->deregister_observer(*observer);else event_stream->deregister_observer(*observer);observer.reset();
  }
  void expected(std::size_t value) const { if(value!=revision)throw StreamingSessionConflict("reader session revision changed; refresh before retrying"); }
  ReaderStreamKind kind;
  EventLog context;
  std::unique_ptr<CsvEventStreamReader> csv;
  std::unique_ptr<XesEventStreamReader> xes;
  std::unique_ptr<XesTraceStreamReader> traces;
  std::unique_ptr<LiveEventStream> event_stream;
  std::unique_ptr<LiveTraceStream> trace_stream;
  mutable std::mutex api_mutex,data_mutex;
  std::optional<std::size_t> observer;
  std::size_t revision=0,generation=0,reader_position=0;
  std::vector<Item> items;
  std::vector<Trace> deliveries;
  std::vector<Batch> batches;
  std::set<std::string> batch_ids;
};
ReaderStreamSession::ReaderStreamSession(ReaderStreamOptions options):impl_(std::make_unique<Impl>(std::move(options))) {}
ReaderStreamSession::~ReaderStreamSession()=default;
std::size_t ReaderStreamSession::revision() const {std::lock_guard lock(impl_->api_mutex);return impl_->revision;}
void ReaderStreamSession::read(std::size_t maximum, std::string batch_id, std::size_t expected_revision) {
  auto& s=*impl_;std::lock_guard api(s.api_mutex);s.expected(expected_revision);
  if(maximum==0||maximum>10000)throw std::invalid_argument("reader batch size must be from 1 through 10000");
  if(batch_id.empty())throw std::invalid_argument("reader batch ID is empty");
  if(s.batch_ids.contains(batch_id))throw StreamingSessionConflict("reader batch ID already committed");
  if(s.state()==StreamingStreamState::kFinished)throw StreamingSessionConflict("finished reader stream cannot accept items; create a new session");
  const auto first=s.items.size(),position=s.reader_position;
  for(std::size_t i=0;i<maximum;++i) {
    std::optional<Trace> trace;
    if(s.traces)trace=s.traces->read_trace();
    else {auto event=s.csv?s.csv->read_event():s.xes->read_event();if(event)trace=event_trace(*event);}
    if(!trace)break;
    // Keep the original native item separate from the queue copy. No synthetic
    // transport fields are injected into source attributes or input_order.
    s.items.push_back({s.items.size(),s.generation,s.reader_position++,batch_id,*trace});
    const bool appended=s.trace_stream?s.trace_stream->append(*trace):s.event_stream->append(trace->events.front());
    if(!appended)throw std::logic_error("native reader stream rejected an item");
  }
  s.batches.push_back({batch_id,s.generation,position,first,s.items.size()-first});s.batch_ids.insert(std::move(batch_id));++s.revision;
}
void ReaderStreamSession::action(ReaderStreamAction action,std::size_t expected_revision) {
  auto& s=*impl_;std::lock_guard api(s.api_mutex);s.expected(expected_revision);
  if(s.state()==StreamingStreamState::kActive)s.drain();
  if(action==ReaderStreamAction::kStart)s.start();
  else if(action==ReaderStreamAction::kDrain)s.drain();
  else if(action==ReaderStreamAction::kFinish) {
    if(s.state()==StreamingStreamState::kInactive&&s.pending())throw std::invalid_argument("start the queued reader stream before finishing");s.stop();
  } else if(action==ReaderStreamAction::kRegister)s.register_observer();
  else if(action==ReaderStreamAction::kDeregister)s.deregister_observer();
  else {if(s.traces)s.traces->reset();else if(s.csv)s.csv->reset();else s.xes->reset();s.reader_position=0;++s.generation;}
  ++s.revision;
}
EventLog ReaderStreamSession::selected_log(bool delivered,bool wait) {
  auto& s=*impl_;std::lock_guard api(s.api_mutex);if(wait&&s.state()==StreamingStreamState::kActive)s.drain();
  std::lock_guard data(s.data_mutex);EventLog log;copy_event_log_context(s.context,log);
  if(delivered)log.traces=s.deliveries;else for(const auto& item:s.items)log.traces.push_back(item.trace);
  if(!s.traces) {
    // Group the case projections without converting the events through flat
    // fields. Conversion would infer new date/type metadata for plain native
    // projections and could change the item returned by the reader.
    EventLog grouped;copy_event_log_context(s.context,grouped);
    std::map<std::tuple<std::string,FirstClassAttributeState,std::string>,std::size_t> positions;
    for(const auto& item:log.traces) {
      const auto key=std::tuple(item.case_id,item.case_id_state,item.case_id_source_key);
      auto [it,inserted]=positions.emplace(key,grouped.traces.size());
      if(inserted){auto trace=item;trace.events.clear();grouped.traces.push_back(std::move(trace));}
      auto& events=grouped.traces[it->second].events;events.insert(events.end(),item.events.begin(),item.events.end());
    }
    return grouped;
  }
  return log;
}
void ReaderStreamSession::wait_until_drained() {
  auto& s=*impl_;std::lock_guard api(s.api_mutex);if(s.state()==StreamingStreamState::kActive)s.drain();
}
std::string ReaderStreamSession::snapshot_json(bool wait) {
  auto& s=*impl_;std::lock_guard api(s.api_mutex);if(wait&&s.state()==StreamingStreamState::kActive)s.drain();
  std::lock_guard data(s.data_mutex);const auto state=s.state();std::ostringstream out;
  out<<"{\"family\":\"STREAM_READER_SESSION\",\"kind\":"<<json_quote(s.traces?"xes_traces":s.csv?"csv_events":"xes_events")<<",\"state\":"<<json_quote(state==StreamingStreamState::kInactive?"INACTIVE":state==StreamingStreamState::kActive?"ACTIVE":"FINISHED")<<",\"revision\":"<<s.revision<<",\"pending\":"<<s.pending()<<",\"enqueued\":"<<s.items.size()<<",\"delivered\":"<<s.deliveries.size()<<",\"registered\":"<<(s.observer?"true":"false")<<",\"observer_id\":"<<(s.observer?std::to_string(*s.observer):"null")<<",\"reader_generation\":"<<s.generation<<",\"reader_position\":"<<s.reader_position<<",\"exhausted\":"<<(s.exhausted()?"true":"false")<<",\"items\":[";
  for(std::size_t i=0;i<s.items.size();++i) {const auto& item=s.items[i];if(i)out<<',';out<<"{\"position\":"<<item.position<<",\"generation\":"<<item.generation<<",\"reader_position\":"<<item.reader_position<<",\"batch_id\":"<<json_quote(item.batch)<<",\"native_item\":"<<native_item(item.trace,s.context)<<'}';}
  out<<"],\"deliveries\":[";
  for(std::size_t i=0;i<s.deliveries.size();++i) {if(i)out<<',';out<<"{\"position\":"<<i<<",\"native_item\":"<<native_item(s.deliveries[i],s.context)<<'}';}
  out<<"],\"batches\":[";
  for(std::size_t i=0;i<s.batches.size();++i) {const auto& batch=s.batches[i];if(i)out<<',';out<<"{\"batch_id\":"<<json_quote(batch.id)<<",\"generation\":"<<batch.generation<<",\"reader_position\":"<<batch.reader_position<<",\"first_position\":"<<batch.first<<",\"items\":"<<batch.count<<'}';}
  out<<"]}";return out.str();
}
}  // namespace minera
