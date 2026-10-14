#include "minera/workflow_input_result_json.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include "minera/workflow_discovery_input_json.hpp"

namespace minera {
namespace {

void text(std::ostream& out, std::string_view value) {
  constexpr char hex[] = "0123456789abcdef";
  out << '"';
  for (std::size_t i = 0; i < value.size();) {
    const auto first = static_cast<unsigned char>(value[i]);
    if (first < 0x80) {
      if (first == '"' || first == '\\') out << '\\' << static_cast<char>(first);
      else if (first < 0x20) out << "\\u00" << hex[first >> 4] << hex[first & 15];
      else out << static_cast<char>(first);
      ++i;
      continue;
    }
    const std::size_t width = first >= 0xc2 && first <= 0xdf ? 2
        : first >= 0xe0 && first <= 0xef ? 3
        : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
    if (!width || i + width > value.size())
      throw std::invalid_argument("workflow result: invalid UTF-8 string");
    std::uint32_t code = first & (width == 2 ? 0x1f : width == 3 ? 0x0f : 0x07);
    for (std::size_t j = 1; j < width; ++j) {
      const auto next = static_cast<unsigned char>(value[i + j]);
      if ((next & 0xc0) != 0x80)
        throw std::invalid_argument("workflow result: invalid UTF-8 string");
      code = (code << 6) | (next & 0x3f);
    }
    if (code < (width == 2 ? 0x80u : width == 3 ? 0x800u : 0x10000u) ||
        code > 0x10ffffu)
      throw std::invalid_argument("workflow result: invalid UTF-8 string");
    if (code >= 0xd800u && code <= 0xdfffu) {
      out << "\\u" << hex[(code >> 12) & 15] << hex[(code >> 8) & 15]
          << hex[(code >> 4) & 15] << hex[code & 15];
    } else {
      out.write(value.data() + i, static_cast<std::streamsize>(width));
    }
    i += width;
  }
  out << '"';
}

void boolean(std::ostream& out, bool value) { out << (value ? "true" : "false"); }

void real(std::ostream& out, double value) {
  if (std::isnan(value)) { text(out, "NaN"); return; }
  if (std::isinf(value)) { text(out, std::signbit(value) ? "-INF" : "INF"); return; }
  std::ostringstream scalar;
  scalar.imbue(std::locale::classic());
  scalar << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  auto encoded = scalar.str();
  if (encoded.find_first_of(".eE") == std::string::npos) encoded += ".0";
  out << encoded;
}

template <typename Range, typename Emit>
void array(std::ostream& out, const Range& values, Emit emit) {
  out << '[';
  bool first = true;
  for (const auto& value : values) {
    if (!first) out << ',';
    first = false;
    emit(value);
  }
  out << ']';
}

void ids(std::ostream& out, const std::vector<std::size_t>& values) {
  array(out, values, [&](auto value) { out << value; });
}

void names(std::ostream& out, const std::vector<std::string>& values) {
  array(out, values, [&](const auto& value) { text(out, value); });
}

void weights(std::ostream& out, const std::vector<PetriArcWeight>& values,
             std::size_t endpoints) {
  if (!values.empty() && values.size() != endpoints)
    throw std::invalid_argument("workflow result: Petri weight dimensions differ");
  array(out, values, [&](const auto& value) {
    if (value.magnitude < 0)
      throw std::invalid_argument("workflow result: Petri weight magnitude is negative");
    out << "{\"negative\":";
    boolean(out, value.negative);
    out << ",\"magnitude\":" << value.magnitude.decimal() << '}';
  });
}

void markings(std::ostream& out, const std::vector<PetriMarkingEntry>& entries,
              std::size_t places, const std::set<std::size_t>& detached) {
  std::set<std::pair<PetriMarkingPlaceKind, std::size_t>> keys;
  array(out, entries, [&](const auto& entry) {
    const bool root = entry.place.kind == PetriMarkingPlaceKind::kRoot;
    if ((!root && entry.place.kind != PetriMarkingPlaceKind::kDetached) ||
        (root && entry.place.id >= places) ||
        (!root && !detached.contains(entry.place.id)) ||
        !keys.emplace(entry.place.kind, entry.place.id).second)
      throw std::invalid_argument("workflow result: invalid sparse Petri marking identity");
    out << "{\"namespace\":\"" << (root ? "root" : "detached")
        << "\",\"place_id\":" << entry.place.id
        << ",\"tokens\":" << entry.tokens.decimal() << '}';
  });
}

void performance(std::ostream& out,
                 const std::optional<HeuristicPerformanceValue>& value) {
  if (value) out << serialize_workflow_performance_value_json(*value);
  else out << "null";
}

void frequency_dfg(std::ostream& out, const HeuristicFrequencyDfg& values) {
  array(out, values, [&](const auto& item) {
    const auto& [pair, count] = item;
    out << "{\"source\":"; text(out, pair.first);
    out << ",\"target\":"; text(out, pair.second);
    out << ",\"count\":" << count << '}';
  });
}

void mean_dfg(std::ostream& out, const HeuristicMeanPerformanceDfg& values) {
  array(out, values, [&](const auto& item) {
    const auto& [pair, value] = item;
    out << "{\"source\":"; text(out, pair.first);
    out << ",\"target\":"; text(out, pair.second);
    out << ",\"value\":"; real(out, value); out << '}';
  });
}

void boundary(std::ostream& out, const HeuristicBoundaryActivities& value) {
  std::visit([&](const auto& selected) {
    using Value = std::decay_t<decltype(selected)>;
    if constexpr (std::is_same_v<Value, std::vector<std::string>>) {
      names(out, selected);
    } else {
      out << '{'; bool first = true;
      for (const auto& [name, count] : selected) {
        if (!first) out << ','; first = false;
        text(out, name); out << ':' << count;
      }
      out << '}';
    }
  }, value);
}

const char* aggregation(HeuristicPerformanceAggregation value) {
  switch (value) {
    case HeuristicPerformanceAggregation::kMean: return "mean";
    case HeuristicPerformanceAggregation::kMedian: return "median";
    case HeuristicPerformanceAggregation::kMinimum: return "min";
    case HeuristicPerformanceAggregation::kMaximum: return "max";
    case HeuristicPerformanceAggregation::kStandardDeviation: return "stdev";
    case HeuristicPerformanceAggregation::kSum: return "sum";
    case HeuristicPerformanceAggregation::kRawValues: return "raw_values";
    case HeuristicPerformanceAggregation::kAll: return "all";
  }
  throw std::invalid_argument("workflow result: invalid Heuristic aggregation");
}

}  // namespace

std::string workflow_input_petri_json(const PetriNet& net) {
  const auto places = net.place_count();
  if ((net.has_source() && net.source() >= places) ||
      (net.has_sink() && net.sink() >= places))
    throw std::invalid_argument("workflow result: invalid Petri boundary place");
  const auto& transition_names = net.transition_names();
  if (transition_names && transition_names->size() != net.transitions().size())
    throw std::invalid_argument("workflow result: Petri display-name dimensions differ");
  std::ostringstream out; out.imbue(std::locale::classic());
  out << "{\"activity_names\":"; names(out, net.activities());
  out << ",\"place_count\":" << places << ",\"has_source\":"; boolean(out, net.has_source());
  out << ",\"has_sink\":"; boolean(out, net.has_sink());
  out << ",\"source\":"; if (net.has_source()) out << net.source(); else out << "null";
  out << ",\"sink\":"; if (net.has_sink()) out << net.sink(); else out << "null";
  out << ",\"transitions\":[";
  for (std::size_t index = 0; index < net.transitions().size(); ++index) {
    if (index) out << ',';
    const auto& transition = net.transitions()[index];
    if (transition.activity && *transition.activity >= net.activities().size())
      throw std::invalid_argument("workflow result: invalid Petri activity index");
    const auto arcs = [&](std::string_view name, const auto& endpoints) {
      for (auto place : endpoints) if (place >= places)
        throw std::invalid_argument("workflow result: invalid Petri arc place");
      out << ','; text(out, name); out << ':'; ids(out, endpoints);
    };
    out << "{\"index\":" << index << ",\"activity_id\":";
    if (transition.activity) out << *transition.activity; else out << "null";
    out << ",\"activity_name\":";
    if (transition.activity) text(out, net.activities()[*transition.activity]); else out << "null";
    out << ",\"activity_is_null\":"; boolean(out, !transition.activity);
    out << ",\"display_name_supplied\":"; boolean(out, transition_names.has_value());
    out << ",\"display_name\":";
    if (transition_names && (*transition_names)[index]) text(out, *(*transition_names)[index]); else out << "null";
    arcs("inputs", transition.inputs); arcs("outputs", transition.outputs);
    arcs("reset_inputs", transition.reset_inputs); arcs("inhibitor_inputs", transition.inhibitor_inputs);
    out << ",\"input_weights\":"; weights(out, transition.input_weights, transition.inputs.size());
    out << ",\"output_weights\":"; weights(out, transition.output_weights, transition.outputs.size());
    out << ",\"reset_weights\":"; weights(out, transition.reset_weights, transition.reset_inputs.size());
    out << ",\"inhibitor_weights\":"; weights(out, transition.inhibitor_weights, transition.inhibitor_inputs.size());
    out << '}';
  }
  out << "],\"discovery_markings\":";
  if (!net.discovery_markings()) out << "null";
  else {
    const auto& value = *net.discovery_markings();
    std::set<std::size_t> detached_ids;
    out << "{\"detached_places\":";
    array(out, value.detached_places, [&](const auto& place) {
      if (!detached_ids.insert(place.id).second)
        throw std::invalid_argument("workflow result: duplicate detached Petri identity");
      out << "{\"id\":" << place.id << ",\"name\":"
          << serialize_workflow_structured_value_json(place.name) << '}';
    });
    out << ",\"initial\":"; markings(out, value.initial, places, detached_ids);
    out << ",\"final\":"; markings(out, value.final, places, detached_ids); out << '}';
  }
  out << '}';
  return out.str();
}

std::string workflow_input_heuristics_json(const HeuristicsNet& net) {
  if (net.variant != HeuristicMinerVariant::kClassic && net.variant != HeuristicMinerVariant::kPlusPlus)
    throw std::invalid_argument("workflow result: invalid Heuristic variant");
  if (net.decoration != HeuristicNetDecoration::kFrequency && net.decoration != HeuristicNetDecoration::kPerformance)
    throw std::invalid_argument("workflow result: invalid Heuristic decoration");
  std::ostringstream out; out.imbue(std::locale::classic());
  out << "{\"variant\":\"" << (net.variant == HeuristicMinerVariant::kClassic ? "classic" : "plusplus")
      << "\",\"decoration\":\"" << (net.decoration == HeuristicNetDecoration::kFrequency ? "frequency" : "performance")
      << "\",\"performance_aggregation\":\"" << aggregation(net.performance_aggregation) << "\",\"activity_attribute\":";
  text(out, net.activity_attribute); out << ",\"timestamp_attribute\":"; text(out, net.timestamp_attribute);
  out << ",\"activity_ids_are_local\":"; boolean(out, net.activity_ids_are_local);
  out << ",\"activities\":"; names(out, net.activities);
  out << ",\"edges\":";
  array(out, net.edges, [&](const auto& edge) {
    out << "{\"source\":" << edge.source << ",\"target\":" << edge.target << ",\"dependency\":"; real(out, edge.dependency);
    out << ",\"frequency\":" << edge.frequency << ",\"representation\":"; real(out, edge.representation);
    out << ",\"length_two_loop\":"; boolean(out, edge.length_two_loop);
    out << ",\"representation_value\":"; performance(out, edge.representation_value); out << '}';
  });
  out << ",\"start_activities\":"; ids(out, net.start_activities);
  out << ",\"end_activities\":"; ids(out, net.end_activities);
  out << ",\"active_activities\":"; ids(out, net.active_activities);
  const auto bundles = [&](const auto& values) {
    array(out, values, [&](const auto& group) { array(out, group, [&](const auto& bundle) { ids(out, bundle); }); });
  };
  out << ",\"output_bundles\":"; bundles(net.output_bundles);
  out << ",\"input_bundles\":"; bundles(net.input_bundles);
  out << ",\"activity_statistics\":";
  array(out, net.activity_statistics, [&](const auto& value) {
    out << "{\"activity\":" << value.activity << ",\"occurrences\":" << value.occurrences.to_string()
        << ",\"start_occurrences\":" << value.start_occurrences << ",\"end_occurrences\":" << value.end_occurrences
        << ",\"mean_sojourn_seconds\":"; real(out, value.mean_sojourn_seconds);
    out << ",\"is_start_activity\":"; if (value.is_start_activity) boolean(out, *value.is_start_activity); else out << "null";
    out << ",\"is_end_activity\":"; if (value.is_end_activity) boolean(out, *value.is_end_activity); else out << "null"; out << '}';
  });
  out << ",\"sojourn_times\":";
  if (!net.sojourn_times) out << "null";
  else {
    out << '{'; bool first = true;
    for (const auto& [name, value] : *net.sojourn_times) {
      if (!first) out << ','; first = false; text(out, name); out << ':'; real(out, value);
    }
    out << '}';
  }
  out << ",\"relation_statistics\":";
  array(out, net.relation_statistics, [&](const auto& value) {
    out << "{\"source\":" << value.source << ",\"target\":" << value.target
        << ",\"eventually_follows_frequency\":" << value.eventually_follows_frequency
        << ",\"direct_adjacency_frequency\":" << value.direct_adjacency_frequency
        << ",\"mean_wait_seconds\":"; real(out, value.mean_wait_seconds);
    out << ",\"raw_frequency\":" << value.raw_frequency << ",\"cleaned_frequency\":" << value.cleaned_frequency
        << ",\"representation\":"; real(out, value.representation);
    out << ",\"length_two_loop\":"; boolean(out, value.length_two_loop);
    out << ",\"wait_samples\":"; array(out, value.wait_samples, [&](double sample) { real(out, sample); });
    out << ",\"median_wait_seconds\":"; real(out, value.median_wait_seconds);
    out << ",\"min_wait_seconds\":"; real(out, value.min_wait_seconds);
    out << ",\"max_wait_seconds\":"; real(out, value.max_wait_seconds);
    out << ",\"stdev_wait_seconds\":"; real(out, value.stdev_wait_seconds);
    out << ",\"sum_wait_seconds\":"; real(out, value.sum_wait_seconds);
    out << ",\"representation_value\":"; performance(out, value.representation_value); out << '}';
  });
  out << ",\"concurrency_statistics\":";
  array(out, net.concurrency_statistics, [&](const auto& value) {
    out << "{\"first\":" << value.first << ",\"second\":" << value.second << ",\"frequency\":" << value.frequency << '}';
  });
  out << ",\"raw_activity_statistics\":";
  array(out, net.raw_activity_statistics, [&](const auto& value) {
    out << "{\"activity\":"; text(out, value.activity); out << ",\"occurrences\":" << value.occurrences << ",\"mean_sojourn_seconds\":"; real(out, value.mean_sojourn_seconds); out << '}';
  });
  out << ",\"raw_concurrency_statistics\":";
  array(out, net.raw_concurrency_statistics, [&](const auto& value) {
    out << "{\"first\":"; text(out, value.first); out << ",\"second\":"; text(out, value.second); out << ",\"frequency\":" << value.frequency << '}';
  });
  out << ",\"raw_dfg\":"; frequency_dfg(out, net.raw_dfg);
  out << ",\"cleaned_dfg\":"; frequency_dfg(out, net.cleaned_dfg);
  out << ",\"window_two_dfg\":"; frequency_dfg(out, net.window_two_dfg);
  out << ",\"frequency_triples\":";
  array(out, net.frequency_triples, [&](const auto& item) {
    const auto& [triple, count] = item; const auto& [first, middle, last] = triple;
    out << "{\"first\":"; text(out, first); out << ",\"middle\":"; text(out, middle); out << ",\"last\":"; text(out, last); out << ",\"count\":" << count << '}';
  });
  const auto measures = [&](const auto& values) {
    array(out, values, [&](const auto& value) {
      out << "{\"activity\":" << value.activity << ",\"first\":" << value.first << ",\"second\":" << value.second << ",\"value\":"; real(out, value.value); out << '}';
    });
  };
  out << ",\"and_measures_in\":"; measures(net.and_measures_in);
  out << ",\"and_measures_out\":"; measures(net.and_measures_out);
  out << ",\"dependency_measures\":";
  array(out, net.dependency_measures, [&](const auto& value) {
    out << "{\"source\":" << value.source << ",\"target\":" << value.target << ",\"value\":"; real(out, value.value); out << '}';
  });
  out << ",\"loop_length_two\":";
  array(out, net.loop_length_two, [&](const auto& value) {
    out << "{\"source\":" << value.source << ",\"target\":" << value.target << ",\"frequency\":" << value.frequency << '}';
  });
  out << ",\"metadata\":";
  if (!net.metadata) out << "null";
  else {
    out << "{\"activities\":"; names(out, net.metadata->activities); out << ",\"activities_occurrences\":{";
    bool first = true;
    for (const auto& [name, count] : net.metadata->activities_occurrences) {
      if (!first) out << ','; first = false; text(out, name); out << ':' << count.to_string();
    }
    out << "},\"start_activities\":"; boundary(out, net.metadata->start_activities);
    out << ",\"end_activities\":"; boundary(out, net.metadata->end_activities); out << '}';
  }
  out << ",\"performance_values\":";
  if (!net.performance_values) out << "null";
  else array(out, *net.performance_values, [&](const auto& item) {
    const auto& [pair, value] = item; out << "{\"source\":"; text(out, pair.first); out << ",\"target\":"; text(out, pair.second);
    out << ",\"value\":" << serialize_workflow_performance_value_json(value) << '}';
  });
  out << ",\"classic_public_dfg_window_2_present\":"; boolean(out, net.classic_public_dfg_window_2_present);
  out << ",\"classic_public_freq_triples_present\":"; boolean(out, net.classic_public_freq_triples_present);
  out << ",\"plusplus_public_dfg_window_2_present\":"; boolean(out, net.plusplus_public_dfg_window_2_present);
  out << ",\"plusplus_public_freq_triples_present\":"; boolean(out, net.plusplus_public_freq_triples_present);
  out << ",\"plusplus_computed_performance_dfg\":";
  if (net.plusplus_computed_performance_dfg) mean_dfg(out, *net.plusplus_computed_performance_dfg); else out << "null";
  out << ",\"plusplus_computed_performance_matrix\":"; mean_dfg(out, net.plusplus_computed_performance_matrix);
  out << '}';
  return out.str();
}

}  // namespace minera
