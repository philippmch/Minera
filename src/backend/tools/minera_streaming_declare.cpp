#include "minera/declare_streaming_io.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
int main(int argc,char** argv) {
  try {
    if(argc<3){std::cerr<<"Usage: minera_streaming_declare MODEL.json EVENTS.csv|EVENTS.xes[.gz] [--format csv|xes] [--order input|imported] [--case-id-column NAME] [--activity-column NAME] [--timestamp-column NAME]\n";return 2;}
    std::string format="csv",order="input";minera::CsvImportOptions csv;
    const std::string path=argv[2];if(path.ends_with(".xes")||path.ends_with(".xes.gz"))format="xes";
    for(int i=3;i<argc;++i){const std::string key=argv[i];if(++i>=argc)throw std::invalid_argument("missing option value");const std::string value=argv[i];if(key=="--format")format=value;else if(key=="--order")order=value;else if(key=="--case-id-column")csv.case_id_column=value;else if(key=="--activity-column")csv.activity_column=value;else if(key=="--timestamp-column")csv.timestamp_column=value;else throw std::invalid_argument("unknown option");}
    if(order!="input"&&order!="imported")throw std::invalid_argument("order expects input or imported");std::ifstream document(argv[1],std::ios::binary);if(!document)throw std::invalid_argument("could not read Declare model");std::ostringstream body;body<<document.rdbuf();const auto model=minera::parse_declare_stream_model_json(body.str());minera::DeclareStreamingConformance monitor(model);minera::EventLog log;
    if(format=="csv")log=minera::import_csv_event_log(argv[2],csv);else if(format=="xes")log=minera::import_xes_event_log(argv[2]);else throw std::invalid_argument("format expects csv or xes");std::vector<const minera::Event*> events;for(const auto& trace:log.traces)for(const auto& event:trace.events)events.push_back(&event);if(order=="input")std::stable_sort(events.begin(),events.end(),[](const auto* a,const auto* b){return a->input_order<b->input_order;});
    std::ostringstream rows;rows<<'[';for(std::size_t i=0;i<events.size();++i){if(i)rows<<',';rows<<minera::serialize_declare_stream_event_json(monitor.receive(*events[i]));}rows<<']';auto output=minera::serialize_declare_streaming_json(monitor.snapshot(),model);output.pop_back();std::cout<<output<<",\"event_results\":"<<rows.str()<<",\"input\":{\"event_order\":\""<<order<<"\",\"parsed_events\":"<<events.size()<<"}}\n";
  }catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}return 0;
}
