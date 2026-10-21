#include "log_attributes_filter_test_support.hpp"
#include <iostream>
int main(){try{
  log_attributes_test::workflows([](const auto& parts){return minera::execute_log_attributes_filter(parts);});
  const auto response=minera::execute_log_attributes_filter({}, {}, "incomplete multipart");
  log_attributes_test::require(response.status==400,"transport error accepted");
  std::cout<<"complete log attributes workflow controls and artifacts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
