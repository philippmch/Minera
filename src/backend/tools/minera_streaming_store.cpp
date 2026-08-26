#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/streaming_store.hpp"

namespace {

minera::StreamingDictionaryVariant parse_variant(std::string_view value) {
  if (value == "classic") return minera::StreamingDictionaryVariant::kClassic;
  if (value == "thread_safe" || value == "thread-safe")
    return minera::StreamingDictionaryVariant::kThreadSafe;
  if (value == "redis") return minera::StreamingDictionaryVariant::kRedis;
  throw std::invalid_argument("variant expects classic, thread_safe, or redis");
}

std::uint64_t parse_unsigned(std::string_view value, std::string_view option,
                             std::uint64_t maximum) {
  if (value.empty() || value.front() == '-')
    throw std::invalid_argument(std::string(option) + " expects an unsigned integer");
  const std::string text(value);
  char* end = nullptr;
  const auto parsed = std::strtoull(text.c_str(), &end, 10);
  if (end != text.c_str() + text.size() || parsed > maximum)
    throw std::invalid_argument(std::string(option) + " is out of range");
  return parsed;
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " classic|thread_safe|redis SCRIPT.txt"
               " [--host HOST] [--port N] [--database N]"
               " [--prefix TEXT] [--timeout-ms N]\n"
               "Commands: set KEY VALUE, get KEY, del KEY, keys, values, size,"
               " clear, flushdb, flushall, ping\n";
  return 2;
}

std::string trim_left(std::string value) {
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) return {};
  value.erase(0, first);
  return value;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 3) return usage(argv[0]);
    minera::StreamingDictionaryOptions options;
    options.variant = parse_variant(argv[1]);
    const std::string script_path = argv[2];
    for (int i = 3; i < argc; ++i) {
      const std::string_view option(argv[i]);
      if (i + 1 >= argc) return usage(argv[0]);
      const std::string_view value(argv[++i]);
      if (option == "--host") {
        options.hostname = value;
      } else if (option == "--port") {
        const auto port = parse_unsigned(value, option, 65535);
        if (port == 0) throw std::invalid_argument("--port must be positive");
        options.port = static_cast<std::uint16_t>(port);
      } else if (option == "--database") {
        options.database = static_cast<std::size_t>(
            parse_unsigned(value, option, std::numeric_limits<std::size_t>::max()));
      } else if (option == "--prefix") {
        options.key_prefix = value;
      } else if (option == "--timeout-ms") {
        const auto timeout = parse_unsigned(value, option, 60'000);
        if (timeout == 0) throw std::invalid_argument("--timeout-ms must be positive");
        options.timeout_ms = static_cast<std::uint32_t>(timeout);
      } else {
        return usage(argv[0]);
      }
    }

    std::ifstream input(script_path, std::ios::binary);
    if (!input) throw std::invalid_argument("could not open store script: " + script_path);
    minera::StreamingDictionary dictionary(options);
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
      ++line_number;
      if (!line.empty() && line.back() == '\r') line.pop_back();
      const auto content = trim_left(line);
      if (content.empty() || content.front() == '#') continue;
      std::istringstream command(content);
      std::string operation;
      command >> operation;
      if (operation == "set") {
        std::string key;
        command >> key;
        std::string value;
        std::getline(command, value);
        value = trim_left(std::move(value));
        if (key.empty()) throw std::invalid_argument("set requires a key");
        dictionary.set(key, value);
      } else if (operation == "get") {
        std::string key;
        command >> key;
        if (key.empty()) throw std::invalid_argument("get requires a key");
        const auto value = dictionary.get(key);
        std::cout << key << '=' << (value ? *value : "<missing>") << '\n';
      } else if (operation == "del") {
        std::string key;
        command >> key;
        if (key.empty()) throw std::invalid_argument("del requires a key");
        std::cout << (dictionary.erase(key) ? "1" : "0") << '\n';
      } else if (operation == "keys") {
        for (const auto& key : dictionary.keys()) std::cout << key << '\n';
      } else if (operation == "values") {
        for (const auto& value : dictionary.values()) std::cout << value << '\n';
      } else if (operation == "size") {
        std::cout << dictionary.size() << '\n';
      } else if (operation == "clear") {
        dictionary.clear();
      } else if (operation == "flushdb") {
        dictionary.flush_database();
      } else if (operation == "flushall") {
        dictionary.flush_all();
      } else if (operation == "ping") {
        std::cout << (dictionary.available() ? "PONG" : "UNAVAILABLE") << '\n';
      } else {
        throw std::invalid_argument("unknown store command on line " +
                                    std::to_string(line_number));
      }
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
