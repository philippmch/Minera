#include "minera/ocel_query.hpp"

#include <stdexcept>
#include <utility>

namespace minera {

std::vector<std::string> split_ocel_query_value(std::string_view value,
                                                char delimiter,
                                                bool final_level) {
  if (delimiter != ',' && delimiter != ':' && delimiter != ';' &&
      delimiter != '|') {
    throw std::invalid_argument("OCEL query delimiter is unsupported");
  }

  std::vector<std::string> result;
  std::string current;
  bool escaped = false;
  for (const char character : value) {
    if (escaped) {
      if (character != '\\' && character != ',' && character != ':' &&
          character != ';' && character != '|') {
        throw std::invalid_argument(
            "OCEL query values use comma, colon, semicolon, pipe, or "
            "backslash escapes");
      }
      if (character == '\\') {
        current.push_back('\\');
        if (!final_level) current.push_back('\\');
      } else if (character == delimiter || final_level) {
        current.push_back(character);
      } else {
        current.push_back('\\');
        current.push_back(character);
      }
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == delimiter) {
      result.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(character);
    }
  }
  if (escaped) throw std::invalid_argument("OCEL query value has dangling escape");
  result.push_back(std::move(current));
  return result;
}

}  // namespace minera
