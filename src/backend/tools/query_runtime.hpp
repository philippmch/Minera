#pragma once
#include "llm_runtime.hpp"
#include <charconv>
#include <cctype>
#include <vector>

namespace minera::query_runtime {
inline httplib::Result request(std::string_view url,std::string_view route,const std::string* body=nullptr){
  llm_runtime::validate_url(url);httplib::Client client{std::string(url)};
  client.set_connection_timeout(std::chrono::seconds(5));client.set_write_timeout(std::chrono::seconds(10));client.set_read_timeout(std::chrono::seconds(60));
  // Stop reading as soon as the body passes the result bound, whatever its framing.
  client.set_payload_max_length(16u*1024u*1024u);
  if(body&&body->size()>32u*1024u*1024u)throw std::length_error("query runtime input exceeds 32 MiB");
  auto result=body?client.Post(std::string(route),*body,"application/json"):client.Get(std::string(route));
  if(!result)throw std::runtime_error("could not reach the query runtime");
  if(result->body.size()>16u*1024u*1024u)throw std::length_error("query runtime result exceeds 16 MiB");
  return result;
}
inline std::vector<std::size_t> parse_indices(std::string_view body,std::size_t count){
  std::size_t position=0;const auto space=[&]{while(position<body.size()&&std::isspace(static_cast<unsigned char>(body[position])))++position;};
  const auto token=[&](std::string_view expected){space();if(body.substr(position,expected.size())!=expected)throw std::runtime_error("invalid query selection response");position+=expected.size();};
  token("{");token("\"indices\"");token(":");token("[");std::vector<std::size_t> result;space();
  if(position<body.size()&&body[position]!=']')while(true){
    space();const auto begin=position;while(position<body.size()&&body[position]>='0'&&body[position]<='9')++position;
    std::size_t index=0;const auto parsed=std::from_chars(body.data()+begin,body.data()+position,index);
    if(begin==position||parsed.ec!=std::errc{}||index>=count||result.size()>1000000)throw std::runtime_error("query selection contains an invalid event index");
    result.push_back(index);space();if(position<body.size()&&body[position]==','){++position;continue;}break;
  }
  token("]");token("}");space();if(position!=body.size())throw std::runtime_error("query selection contains trailing data");return result;
}
} // namespace minera::query_runtime
