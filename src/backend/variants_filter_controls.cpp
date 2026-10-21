#include "minera/variants_filter_controls.hpp"
#include "minera/literal_controls_json.hpp"
#include <stdexcept>
namespace minera {
namespace {
EventAttribute strings(std::string_view input) {
  EventAttribute result; result.container=AttributeContainerKind::kList;
  for(const auto& value:parse_literal_string_vector_json(input)) result.children.push_back({std::to_string(result.children.size()),value});
  return result;
}
}
EventAttribute parse_variants_filter_string_list_json(std::string_view input) {return strings(input);}
EventAttribute parse_variants_filter_admitted_json(std::string_view input, bool tuple_sequences) {
  std::size_t at=0;
  const auto space=[&] {while(at<input.size() && (input[at]==' ' || input[at]=='\t' || input[at]=='\r' || input[at]=='\n')) ++at;};
  const auto fail=[] {throw std::invalid_argument("admitted_json must be an array of string arrays or literal variant strings");};
  const auto require=[&](char expected) {space(); if(at==input.size() || input[at++]!=expected) fail();};
  EventAttribute result; result.container=AttributeContainerKind::kList;
  require('['); space();
  if(at<input.size() && input[at]==']') ++at;
  else while(true) {
    space(); if(at==input.size()) fail(); const auto begin=at;
    if(input[at]=='"') {
      ++at; bool escaped=false,closed=false;
      while(at<input.size()) {const char c=input[at++]; if(escaped) escaped=false; else if(c=='\\') escaped=true; else if(c=='"') {closed=true;break;}}
      if(!closed) fail();
      const auto value=parse_literal_string_vector_json("["+std::string(input.substr(begin,at-begin))+"]");
      result.children.push_back({std::to_string(result.children.size()),value.at(0)});
    } else if(input[at]=='[') {
      ++at; bool quoted=false,escaped=false,closed=false; unsigned depth=1;
      while(at<input.size()) {
        const char c=input[at++];
        if(quoted) {if(escaped) escaped=false; else if(c=='\\') escaped=true; else if(c=='"') quoted=false;}
        else if(c=='"') quoted=true;
        else if(c=='[') {if(++depth>2) fail();}
        else if(c==']' && --depth==0) {closed=true;break;}
      }
      if(!closed) fail(); auto sequence=strings(input.substr(begin,at-begin));
      if(tuple_sequences) sequence.container=AttributeContainerKind::kTuple;
      sequence.key=std::to_string(result.children.size()); result.children.push_back(std::move(sequence));
    } else fail();
    space(); if(at<input.size() && input[at]==']') {++at;break;} require(',');
  }
  space(); if(at!=input.size()) fail(); return result;
}
}
