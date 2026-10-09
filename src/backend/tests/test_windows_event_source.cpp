#undef NDEBUG
#include "minera/source_acquisition.hpp"
#include <cassert>
#include <stdexcept>
#include <vector>
using namespace minera;
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
const std::string first="<Event><System><Provider Name=\"Test,東京\"/><EventID>1</EventID><TimeCreated SystemTime=\"2024-01-01T00:00:00Z\"/><Computer>CONTROLLED</Computer></System><EventData><Data Name=\"nested\">A &amp; B</Data></EventData></Event>";
const std::string second="<Event><System><Provider Name=\"Test\"/><EventID>2</EventID><TimeCreated SystemTime=\"2024-01-02T00:00:00Z\"/><Computer>CONTROLLED</Computer></System></Event>";
WindowsEventNext reader(std::vector<std::string> values){return [values=std::move(values),i=std::size_t{0}]()mutable->std::optional<std::string>{if(i==values.size())return std::nullopt;return values[i++];};}
int main(){
 WindowsEventSourceOptions o;o.channel="Controlled";o.max_events=1;auto c=capture_windows_events(o,reader({first,second}));assert(c.events==1&&c.limited&&c.xml=="<Events>\n"+first+"\n</Events>");assert(windows_event_capture_json(c).find("\"limited\":true")!=std::string::npos);
 o.max_events=2;c=capture_windows_events(o,reader({first,second}));assert(c.events==2&&!c.limited);c=capture_windows_events(o,reader({}));assert(c.events==0&&!c.limited&&c.xml=="<Events>\n</Events>");
 SourceAcquisitionInput input;input.connector="windows_events";input.parameters={{"windows_events_source","service"},{"windows_events_channel","Controlled"},{"windows_events_max_events","1"}};input.windows_event_reader=reader({first,second});const auto acquired=acquire_source(input);assert(acquired.log.event_count()==1&&!acquired.original_bytes&&acquired.windows_capture->limited);assert(*acquired.log.traces[0].events[0].attribute("data:nested")=="A & B");assert(source_acquisition_json(acquired).find("windows_event_service")!=std::string::npos);
 input.source_bytes="body";rejects([&]{(void)acquire_source(input);});input.source_bytes.reset();input.parameters["windows_events_reverse"]="yes";rejects([&]{(void)acquire_source(input);});input.parameters.erase("windows_events_reverse");input.parameters["windows_events_query"]="";rejects([&]{(void)acquire_source(input);});
 o.channel="";rejects([&]{(void)capture_windows_events(o,reader({first}));});o.channel="Controlled";o.max_events=0;rejects([&]{(void)capture_windows_events(o,reader({first}));});o.max_events=1;o.timeout_ms=30001;rejects([&]{(void)capture_windows_events(o,reader({first}));});o.timeout_ms=1000;rejects([&]{(void)capture_windows_events(o,reader({""}));});
}
