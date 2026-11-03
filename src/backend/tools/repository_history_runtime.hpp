#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "httplib.h"
#include "minera/repository_history.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <memory>
#include <vector>
#endif

namespace minera::tools {

#ifdef _WIN32
namespace repository_transport_detail {
inline std::wstring wide(std::string_view text) {
  if(text.empty())return {};const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);if(!length)throw std::invalid_argument("Repository history: invalid UTF-8 endpoint/header");std::wstring value(length,L'\0');if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),value.data(),length))throw std::invalid_argument("Repository history: UTF-8 conversion failed");return value;
}
struct Handle {
  HINTERNET value;
  explicit Handle(HINTERNET handle):value(handle){if(!value)throw std::runtime_error("Repository history: HTTP handle failed ("+std::to_string(GetLastError())+")");}
  ~Handle(){WinHttpCloseHandle(value);}
  Handle(const Handle&)=delete;
};
inline void require(bool value){if(!value)throw std::runtime_error("Repository history: HTTP request failed ("+std::to_string(GetLastError())+")");}
inline RepositoryHistoryResponse get(std::string_view url,std::string_view token) {
  const auto endpoint=wide(url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwSchemeLength=parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=parts.dwPasswordLength=static_cast<DWORD>(-1);
  require(WinHttpCrackUrl(endpoint.c_str(),static_cast<DWORD>(endpoint.size()),0,&parts));
  if((parts.nScheme!=INTERNET_SCHEME_HTTP&&parts.nScheme!=INTERNET_SCHEME_HTTPS)||parts.dwUserNameLength||parts.dwPasswordLength)throw std::invalid_argument("Repository history: endpoint requires HTTP(S) without embedded credentials");
  const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);std::wstring path;if(parts.dwUrlPathLength)path.assign(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);if(path.empty())path=L"/";
  Handle session(WinHttpOpen(L"Minera",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));require(WinHttpSetTimeouts(session.value,10000,10000,10000,30000));
  Handle connection(WinHttpConnect(session.value,host.c_str(),parts.nPort,0));Handle request(WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0));
  // Keep system certificate validation. Redirect responses remain responses,
  // rather than forwarding caller credentials to another origin.
  DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;require(WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects)));
  std::wstring headers=L"Accept: application/json\r\n";if(!token.empty()){if(token.find_first_of("\r\n")!=std::string_view::npos)throw std::invalid_argument("Repository history: invalid bearer token header");headers+=L"Authorization: Bearer "+wide(token)+L"\r\n";}
  require(WinHttpSendRequest(request.value,headers.c_str(),static_cast<DWORD>(headers.size()),WINHTTP_NO_REQUEST_DATA,0,0,0));require(WinHttpReceiveResponse(request.value,nullptr));DWORD status=0,size=sizeof(status);require(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX));
  std::string body;char buffer[16384];for(;;){DWORD read=0;require(WinHttpReadData(request.value,buffer,sizeof(buffer),&read));if(!read)break;if(body.size()+read>64ULL*1024*1024)throw std::length_error("Repository history: response exceeds 64 MiB");body.append(buffer,read);}return {static_cast<int>(status),std::move(body)};
}
}  // namespace repository_transport_detail
#endif

inline RepositoryHistoryTransport repository_history_http_transport() {
  return [](std::string_view url, std::string_view token) {
#ifdef _WIN32
    return repository_transport_detail::get(url,token);
#else
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
    // The same 64 MiB response bound as the WinHTTP transport, applied while
    // the body streams in.
    client.set_payload_max_length(64ULL * 1024 * 1024);
    httplib::Headers headers{{"Accept", "application/json"},
                             {"User-Agent", "Minera"}};
    if (!token.empty())
      headers.emplace("Authorization", "Bearer " + std::string(token));
    const auto response = client.Get(path, headers);
    if (!response)
      throw std::invalid_argument("Repository history: endpoint request failed");
    return RepositoryHistoryResponse{response->status, response->body};
#endif
  };
}

}  // namespace minera::tools
