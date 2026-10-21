#include "table_variants_filter_test_support.hpp"
#include <iostream>
using namespace table_variants_test;
int main(){try{
  variants_workflows([](const auto& parts){return execute_table_variants_filter(parts);});
  for(const auto& control:std::vector<DiscoveryWorkbenchPart>{field("engine","pandas"),field("unknown","x"),field("positive","yes"),field("sequence_kind","set"),field("timestamp_key","inactive")})
    require(execute_table_variants_filter(variants_request("pandas",between_source("pandas"),"membership",{control})).status==400,"invalid control accepted");
  require(execute_table_variants_filter(variants_request("pandas",between_source("pandas")),{field("engine","pandas")}).status==400,"duplicate query accepted");
  const auto value=parse_variants_filter_admitted_json("[[\"A,B\",\"\",\"x\\u0000y\"],[]]",true);
  require(value.children.size()==2 && value.children[0].container==AttributeContainerKind::kTuple && value.children[0].children[2].value==std::string("x\0y",3),"literal tuple parser narrowed");
  for(const auto bad:{"null","[1]","[[1]]","[[] ,]","[[[\"A\"]]]","[]x"}){bool caught=false;try{(void)parse_variants_filter_admitted_json(bad);}catch(const std::invalid_argument&){caught=true;}require(caught,"invalid variant JSON accepted");}
  std::cout<<"complete variants table workbench passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
