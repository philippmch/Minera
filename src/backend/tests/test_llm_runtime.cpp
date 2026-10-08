#include <cassert>
#include <string>
#include <thread>
#include "../tools/llm_runtime.hpp"

template<class F> void rejects(F&& function) {
  bool failed=false;
  try { function(); } catch(const std::exception&) { failed=true; }
  assert(failed);
}

int main() {
  for(const auto* url:{"https://127.0.0.1:8189","http://key@localhost:8189","http://localhost:0","http://localhost:8189/path"})
    rejects([&]{minera::llm_runtime::validate_url(url);});
  httplib::Server server;
  std::string captured;
  server.Post("/complete",[&](const httplib::Request& request,httplib::Response& response){captured=request.body;response.set_content("{\"family\":\"LLM_COMPLETION\",\"text\":\"fixture\"}","application/json");});
  server.Get("/health",[](const httplib::Request&,httplib::Response& response){response.set_content("{\"ready\":true}","application/json");});
  const auto port=server.bind_to_any_port("127.0.0.1");assert(port>0);
  std::thread worker([&]{server.listen_after_bind();});
  server.wait_until_ready();assert(server.is_running());
  const auto url="http://127.0.0.1:"+std::to_string(port);
  const auto health=minera::llm_runtime::request(url,"/health");assert(health->status==200);
  minera::llm_runtime::CompletionOptions options;options.model="fixture";options.temperature=0.25;
  const auto result=minera::llm_runtime::completion_json(url,"quote \" / line\nα",options);
  assert(result.find("fixture")!=std::string::npos);
  assert(captured.find("quote \\\"")!=std::string::npos);
  assert(captured.find("\\nα")!=std::string::npos);
  assert(captured.find("\"max_tokens\":4096")!=std::string::npos);
  options.provider="bad";rejects([&]{minera::llm_runtime::completion_json(url,"prompt",options);});
  options.provider="openai";options.model="";rejects([&]{minera::llm_runtime::completion_json(url,"prompt",options);});
  options.model="fixture";options.max_tokens=0;rejects([&]{minera::llm_runtime::completion_json(url,"prompt",options);});
  options.max_tokens=256;options.temperature=2.1;rejects([&]{minera::llm_runtime::completion_json(url,"prompt",options);});
  options.temperature=2;options.openai_api="responses";options.extra_payload="{\"metadata\":{\"source\":\"owned\"}}";
  const auto image_path=std::filesystem::temp_directory_path()/("minera-llm-image-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".png");
  {std::ofstream image(image_path,std::ios::binary);image<<"\x89PNG\r\n\x1a\n";}
  options.images.push_back(minera::llm_runtime::image_file(image_path));std::filesystem::remove(image_path);
  assert(options.images[0].media_type=="image/png"&&options.images[0].data=="iVBORw0KGgo=");
  minera::llm_runtime::completion_json(url,"image prompt",options);
  assert(captured.find("\"openai_api\":\"responses\"")!=std::string::npos&&captured.find("iVBORw0KGgo=")!=std::string::npos);
  assert(captured.find("\\\"metadata\\\"")!=std::string::npos);
  options.provider="anthropic";options.openai_api.clear();rejects([&]{minera::llm_runtime::completion_json(url,"prompt",options);});
  server.stop();worker.join();
}
