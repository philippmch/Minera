#undef NDEBUG
// A separate validation executable uses the production HTTP handlers and native
// extraction with caller-owned COM graphs. No profile override exists in the
// production server or its command-line options.
#define main minera_product_server_main
#include "../tools/minera_server.cpp"
#undef main
#include "outlook_source_fixture.hpp"
namespace {
minera::SourceAcquisitionResult acquire_owned_outlook(const minera::SourceAcquisitionInput& input) {
  if(!input.parameters.contains("outlook_source"))return minera::acquire_source(input);
  const auto code=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(code))throw std::runtime_error("Controlled COM apartment could not initialize");struct Apartment{~Apartment(){CoUninitialize();}} apartment;
  minera::test_outlook::Graph graph;if(input.connector=="outlook_mail_extractor")minera::test_outlook::install_large_mail(graph);
  auto configured=input;configured.outlook_application=graph.application;auto result=minera::acquire_source(configured);graph.balanced();return result;
}
}
int main(int argc,char** argv) {
  if(argc!=3)return 2;const auto port=std::stoi(argv[1]);httplib::Server server;
  server.Post("/api/source-acquisition/acquire",[](const httplib::Request& r,httplib::Response& s){handle_acquire_source_using(r,s,acquire_owned_outlook);});
  server.Get("/api/source-acquisition/snapshot",handle_acquired_snapshot);
  server.Get("/api/source-acquisition/original",handle_acquired_original);
  server.Get("/api/source-acquisition/capture",handle_acquired_capture);
  server.Get("/api/source-acquisition/table",handle_acquired_database_table);
  server.Get("/api/source-acquisition/export",handle_acquired_export);
  server.Get("/api/source-acquisition/list",handle_acquired_list);
  server.Post("/api/source-acquisition/release",handle_acquired_release);
  server.Post("/api/mine",handle_mine);
  server.Get("/api/xes",handle_xes);
  server.Get("/api/health",[](const httplib::Request&,httplib::Response& r){r.set_content("{\"controlled_outlook_fixture\":true}","application/json");});
  server.set_mount_point("/",argv[2]);std::cout<<"Controlled Outlook COM fixture, production handlers, loopback port "<<port<<std::endl;
  return server.listen("127.0.0.1",port)?0:1;
}
