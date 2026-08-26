#include "minera/ocel_query.hpp"

#include <cassert>
#include <stdexcept>

int main() {
  const auto simple = minera::split_ocel_query_value("id\\,1,plain", ',');
  assert((simple == std::vector<std::string>{"id,1", "plain"}));

  const auto escaped = minera::split_ocel_query_value(
      "type\\:order:Approve\\|Reject\\;Pack\\\\Back", ';', false);
  assert(escaped.size() == 1);
  const auto fields = minera::split_ocel_query_value(escaped.front(), ':', false);
  assert(fields.size() == 2 && fields[0] == "type:order");
  const auto activities = minera::split_ocel_query_value(fields[1], '|');
  assert((activities == std::vector<std::string>{"Approve|Reject;Pack\\Back"}));

  bool rejected = false;
  try {
    (void)minera::split_ocel_query_value("dangling\\", ',');
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    (void)minera::split_ocel_query_value("bad\\q", ',');
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  return 0;
}
