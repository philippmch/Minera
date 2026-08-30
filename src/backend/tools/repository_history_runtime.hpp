#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "httplib.h"
#include "minera/repository_history.hpp"

namespace minera::tools {

inline RepositoryHistoryTransport repository_history_http_transport() {
  return [](std::string_view url, std::string_view token) {
    const auto scheme = url.find("://");
    if (scheme == std::string_view::npos)
      throw std::invalid_argument("Repository history: invalid endpoint URL");
    const auto authority_begin = scheme + 3;
    const auto path_begin = url.find('/', authority_begin);
    const std::string origin(url.substr(0, path_begin));
    const std::string path(path_begin == std::string_view::npos
                               ? "/"
                               : url.substr(path_begin));
    httplib::Client client(origin);
    client.set_connection_timeout(10);
    client.set_read_timeout(30);
    client.set_write_timeout(10);
    httplib::Headers headers{{"Accept", "application/json"},
                             {"User-Agent", "Minera"}};
    if (!token.empty())
      headers.emplace("Authorization", "Bearer " + std::string(token));
    const auto response = client.Get(path, headers);
    if (!response)
      throw std::invalid_argument("Repository history: endpoint request failed");
    return RepositoryHistoryResponse{response->status, response->body};
  };
}

}  // namespace minera::tools
