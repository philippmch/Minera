#include "minera/alpha_miner.hpp"
#include "minera/alpha_result_json.hpp"
#include "minera/log_conversion.hpp"
#include <cassert>
#include <stdexcept>

int main() {
  minera::EventStream stream;
  stream.events = {
      {{{"case:concept:name", "c"}, {"concept:name", "A"}}},
      {{{"case:concept:name", "c"}, {"concept:name", "A"}}}};
  const auto table = minera::event_stream_to_table(stream);
  const auto lazy = minera::LazyEventTable::from_table(table);
  const auto verify = [](const minera::AlphaPlusDiscoveryResult& result) {
    assert(result.place_names.size() == result.net.place_count());
    assert(result.initial_marking.size() == result.net.place_count());
    assert(result.final_marking.size() == result.net.place_count());
    assert(result.net.discovery_markings().has_value());
    assert(!result.detached_initial_marking.empty());
    assert(minera::alpha_plus_result_json(result).find("\"detached_initial_marking\":[{") != std::string::npos);
  };
  verify(minera::discover_alpha_plus_result(stream));
  verify(minera::discover_alpha_plus_result(table));
  verify(minera::discover_alpha_plus_result(lazy));
  minera::AlphaMinerOptions options;
  options.max_rows_to_materialize = 1;
  bool rejected = false;
  try { (void)minera::discover_alpha_plus_result(lazy, options); }
  catch (const std::length_error&) { rejected = true; }
  assert(rejected);
}
