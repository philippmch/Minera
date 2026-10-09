#include "minera/reader_stream_session.hpp"
#include "minera/log_conversion.hpp"
#include "minera/xes_export.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

int main(int argc,char** argv) {
  try {
    if(argc<2){std::cerr<<"Usage: minera_reader_stream INPUT --kind csv_events|xes_events|xes_traces [--batch-size N] [--activity-prefix TEXT] [--accept-case TEXT] [--accept-activity TEXT] [--min-trace-events N] [--max-trace-events N] [--output-xes PATH]\n";return 2;}
    minera::ReaderStreamOptions options;std::size_t batch_size=25,minimum=0,maximum=std::numeric_limits<std::size_t>::max();std::string prefix,output_path;std::optional<std::string> case_id,activity;
    const auto size=[](const std::string& value){std::size_t result=0;auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())throw std::invalid_argument("invalid integer option");return result;};
    const std::string path=argv[1];if(path.ends_with(".xes")||path.ends_with(".xes.gz"))options.kind=minera::ReaderStreamKind::kXesEvents;
    for(int i=2;i<argc;++i){const std::string key=argv[i];if(++i>=argc)throw std::invalid_argument("missing option value");const std::string value=argv[i];
      if(key=="--kind"){if(value=="csv_events")options.kind=minera::ReaderStreamKind::kCsvEvents;else if(value=="xes_events")options.kind=minera::ReaderStreamKind::kXesEvents;else if(value=="xes_traces")options.kind=minera::ReaderStreamKind::kXesTraces;else throw std::invalid_argument("unknown reader kind");}
      else if(key=="--batch-size")batch_size=size(value);else if(key=="--activity-prefix")prefix=value;else if(key=="--accept-case")case_id=value;else if(key=="--accept-activity")activity=value;else if(key=="--min-trace-events")minimum=size(value);else if(key=="--max-trace-events")maximum=size(value);else if(key=="--output-xes")output_path=value;
      else if(key=="--case-id-column")options.csv.case_id_column=value;else if(key=="--activity-column")options.csv.activity_column=value;else if(key=="--timestamp-column")options.csv.timestamp_column=value;
      else if(key=="--csv-delimiter"){if(value.size()!=1||value[0]=='\r'||value[0]=='\n'||value[0]=='\0')throw std::invalid_argument("invalid delimiter");options.csv.delimiter=value[0];}
      else if(key=="--xes-parser")options.xes.variant=minera::parse_xes_import_variant(value);else if(key=="--xes-case-attribute")options.xes.case_attribute=value;else if(key=="--xes-activity-attribute")options.xes.activity_attribute=value;else if(key=="--xes-timestamp-attribute")options.xes.timestamp_attribute=value;else throw std::invalid_argument("unknown option: "+key);
    }
    if(batch_size==0||batch_size>10000||minimum>maximum)throw std::invalid_argument("invalid reader bounds");
    std::ifstream input(argv[1],std::ios::binary);if(!input)throw std::invalid_argument("could not read source");std::ostringstream bytes;bytes<<input.rdbuf();options.source=bytes.str();
    if(!prefix.empty()){options.events.transform=[prefix](minera::Event e){auto source=e;minera::set_event_activity_context(source,prefix+source.activity,e);return e;};options.traces.transform=[prefix](minera::Trace t){for(auto& e:t.events){auto source=e;minera::set_event_activity_context(source,prefix+source.activity,e);}return t;};}
    options.events.acceptance=[case_id,activity](const minera::Event& e){return (!case_id||e.case_id==*case_id)&&(!activity||e.activity==*activity);};
    options.traces.acceptance=[case_id,activity,minimum,maximum](const minera::Trace& t){return (!case_id||t.case_id==*case_id)&&t.events.size()>=minimum&&t.events.size()<=maximum&&(!activity||std::any_of(t.events.begin(),t.events.end(),[&](const auto& e){return e.activity==*activity;}));};
    minera::ReaderStreamSession session(std::move(options));session.action(minera::ReaderStreamAction::kStart,0);std::size_t index=0;
    do{session.read(batch_size,"batch-"+std::to_string(index++),session.revision());}while(session.snapshot_json().find("\"exhausted\":true,")==std::string::npos);
    session.action(minera::ReaderStreamAction::kFinish,session.revision());minera::LogConversionResult selected;selected.log=session.selected_log(true);
    if(!output_path.empty()){minera::XesExportOptions xes;xes.preserve_nulls=true;(void)minera::export_xes_event_log(output_path,selected.log,xes);}
    std::cout<<"{\"family\":\"STREAM_READER_CLI\",\"session\":"<<session.snapshot_json()<<",\"selected\":"<<minera::log_conversion_json(selected)<<"}\n";
  }catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}return 0;
}
