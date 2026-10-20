#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "minera/knowledge_injection.hpp"
#include "minera/knowledge_exact_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "llm_runtime.hpp"

namespace {

std::size_t positive_size(std::string_view text, std::string_view option) {
  std::size_t value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value == 0)
    throw std::invalid_argument(std::string(option) + " expects a positive integer");
  return value;
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " --variant description|process_mining|database"
               " [--description TEXT] [--query TEXT] [--database-schema TEXT]"
               " [--max-input-bytes N] [--max-prompt-bytes N]"
               " [--assumptions-json JSON | --assumptions-file PATH]"
               " [--context-json JSON | --context-file PATH]"
               " [--options-json JSON | --options-file PATH]"
               " [--runtime-url URL --provider openai|anthropic|google --model MODEL]"
               " [--max-tokens N] [--temperature N] [--openai-api responses|chat_completions]"
               " [--image PATH] [--image-detail auto|low|high|original]"
               " [--extra-payload JSON | --extra-payload-file PATH]"
               " [--api-key TEXT] [--api-url URL] [--openai-token-field max_completion_tokens|max_tokens]\n";
  std::cerr << "Exact options require all eight native fields. Assumptions are NativeString arrays; context is [NativeString,NativeString] pairs. File controls preserve original bytes. Exact output retains the complete native preview separately from optional explicit runtime completion.\n";
  return 2;
}

}  // namespace

int run(int argc, char** argv) {
  const auto exact_name=[](std::string_view name)->std::string{
    if(name=="--assumptions-json")return "knowledge_assumptions_json";
    if(name=="--assumptions-file")return "knowledge_assumptions_file";
    if(name=="--context-json")return "knowledge_context_json";
    if(name=="--context-file")return "knowledge_context_file";
    if(name=="--options-json")return "knowledge_options_json";
    if(name=="--options-file")return "knowledge_options_file";
    return {};
  };
  bool exact_mode=false;for(int i=1;i<argc;i+=2)if(!exact_name(argv[i]).empty())exact_mode=true;
  std::vector<minera::KnowledgeExactInput> exact_inputs;
  std::optional<minera::KnowledgeInjectionOptions> exact_options;
  std::optional<minera::KnowledgeInjectionResult> exact_result;
  try {
    minera::KnowledgeInjectionOptions options;
    minera::llm_runtime::CompletionOptions completion;
    std::string runtime_url;
    std::string image_detail;
    for (int index = 1; index < argc; ++index) {
      const std::string option = argv[index];
      if (index + 1 >= argc) {if(exact_mode)throw std::invalid_argument(option+" expects a value");return usage(argv[0]);}
      const std::string value = argv[++index];
      if(exact_mode){
        if(const auto field=exact_name(option);!field.empty()){
          if(field.ends_with("_file")){
            const auto path=std::filesystem::u8path(value);std::ifstream input(path,std::ios::binary);
            if(!input)throw std::invalid_argument("cannot open exact knowledge input File");
            std::ostringstream bytes;bytes<<input.rdbuf();const auto filename=path.filename().u8string();
            exact_inputs.push_back({field,std::string(filename.begin(),filename.end()),bytes.str()});
          }else exact_inputs.push_back({field,std::nullopt,value});
          continue;
        }
        std::string field;
        if(option=="--variant")field="knowledge_variant";
        else if(option=="--description")field="knowledge_description";
        else if(option=="--query")field="knowledge_query";
        else if(option=="--database-schema")field="knowledge_database_schema";
        else if(option=="--max-input-bytes")field="knowledge_max_input_bytes";
        else if(option=="--max-prompt-bytes")field="knowledge_max_prompt_bytes";
        if(!field.empty()){exact_inputs.push_back({field,std::nullopt,value});continue;}
      }
      if (option == "--variant")
        options.variant = minera::knowledge_injection_variant_from_string(value);
      else if (option == "--description") options.description = value;
      else if (option == "--query") options.query = value;
      else if (option == "--database-schema") options.database_schema = value;
      else if (option == "--max-input-bytes")
        options.max_input_bytes = positive_size(value, option);
      else if (option == "--max-prompt-bytes")
        options.max_prompt_bytes = positive_size(value, option);
      else if (option == "--runtime-url") runtime_url = value;
      else if (option == "--provider") completion.provider = value;
      else if (option == "--model") completion.model = value;
      else if (option == "--max-tokens") completion.max_tokens = positive_size(value, option);
      else if (option == "--openai-api") completion.openai_api = value;
      else if (option == "--api-key") completion.api_key = value;
      else if (option == "--api-url") completion.api_url = value;
      else if (option == "--openai-token-field") completion.openai_token_field = value;
      else if (option == "--image") completion.images.push_back(minera::llm_runtime::image_file(std::filesystem::u8path(value)));
      else if (option == "--image-detail") image_detail = value;
      else if (option == "--extra-payload") completion.extra_payload = value;
      else if (option == "--extra-payload-file") completion.extra_payload = minera::llm_runtime::bounded_file(std::filesystem::u8path(value));
      else if (option == "--temperature") {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(parsed) || parsed < 0 || parsed > 2)
          throw std::invalid_argument("temperature must be from 0 through 2, with an Anthropic maximum of 1");
        completion.temperature = parsed;
      }
      else return usage(argv[0]);
    }
    if (!image_detail.empty()) {
      if (image_detail != "auto" && image_detail != "low" && image_detail != "high" && image_detail != "original")
        throw std::invalid_argument("invalid OpenAI image detail");
      for (auto& image : completion.images) image.detail = image_detail;
    }
    if(exact_mode){
      exact_options=minera::parse_knowledge_exact_inputs(exact_inputs);
      exact_result=minera::build_knowledge_prompt(*exact_options);
      auto envelope=minera::serialize_knowledge_exact_envelope_json(exact_options,exact_result,exact_inputs);
      if(!runtime_url.empty()){
        if(!exact_result->computed)throw std::runtime_error(exact_result->error);
        if(minera::literal_controls_transport_error(minera::knowledge_injection_json(*exact_result)))throw std::invalid_argument("native prompt is not scalar UTF-8; review a Unicode provider prompt separately");
        const auto completion_json=minera::llm_runtime::completion_json(runtime_url,exact_result->prompt,completion);
        envelope.pop_back();envelope+=",\"completion\":"+completion_json+"}";
      }
      std::cout<<envelope<<'\n';return 0;
    }
    const auto result = minera::build_knowledge_prompt(options);
    if (runtime_url.empty()) std::cout << minera::knowledge_injection_json(result) << '\n';
    else {
      if (!result.computed) throw std::runtime_error(result.error);
      std::cout << minera::llm_runtime::completion_json(runtime_url, result.prompt, completion) << '\n';
    }
    return 0;
  } catch (const std::exception& error) {
    if(exact_mode)std::cout<<minera::serialize_knowledge_exact_envelope_json(exact_options,exact_result,exact_inputs,error.what())<<'\n';
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
  std::vector<std::string> values;
  values.reserve(argc);
  for (int index = 0; index < argc; ++index) {
    const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) { std::cerr << "error: invalid Unicode argument\n"; return 1; }
    std::string value(static_cast<std::size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1, value.data(), size, nullptr, nullptr)) {
      std::cerr << "error: could not convert Unicode argument\n"; return 1;
    }
    value.pop_back(); values.push_back(std::move(value));
  }
  std::vector<char*> arguments;
  for (auto& value : values) arguments.push_back(value.data());
  return run(argc, arguments.data());
}
#else
int main(int argc, char** argv) { return run(argc, argv); }
#endif
