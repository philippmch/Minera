#pragma once

// Included in the server implementation namespace after active_source_route.
// These snapshots describe configured native values, including omitted versus
// engaged-empty optionals. Model-defaulted replay can use the direct overload.

namespace active_source_options_detail {

using Fields = std::initializer_list<std::pair<std::string, std::string>>;

std::string object(Fields fields) {
  std::string result = "{";
  for (const auto& [key, value] : fields) {
    if (result.size() != 1) result += ',';
    result += active_source_quote(key) + ':' + value;
  }
  return result + '}';
}

std::string scalar(bool value) { return value ? "true" : "false"; }
template <typename Integer>
  requires std::is_integral_v<Integer>
std::string scalar(Integer value) { return std::to_string(value); }
std::string scalar(double value) {
  if (std::isnan(value)) return "\"NaN\"";
  if (!std::isfinite(value)) return std::signbit(value) ? "\"-INF\"" : "\"INF\"";
  std::array<char, 64> buffer{};
  const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
      value, std::chars_format::general, std::numeric_limits<double>::max_digits10);
  if (converted.ec != std::errc{}) throw std::runtime_error("configured option float serialization failed");
  std::string result(buffer.data(), converted.ptr);
  if (result.find_first_of(".eE") == std::string::npos) result += ".0";
  return result;
}
std::string scalar(const std::string& value) { return active_source_quote(value); }
std::string scalar(const minera::TokenInteger& value) { return value.decimal(); }

template <typename Range, typename Writer>
std::string array(const Range& values, const Writer& writer) {
  std::string result = "[";
  for (const auto& value : values) {
    if (result.size() != 1) result += ',';
    result += writer(value);
  }
  return result + ']';
}
template <typename Range>
std::string array(const Range& values) {
  return array(values, [](const auto& value) { return scalar(value); });
}
template <typename Value, typename Writer>
std::string optional(const std::optional<Value>& value, const Writer& writer) {
  return value ? writer(*value) : "null";
}
template <typename Value>
std::string optional(const std::optional<Value>& value) {
  return optional(value, [](const auto& entry) { return scalar(entry); });
}
template <typename Enum, std::size_t Count>
std::string enum_name(Enum value, const char* const (&names)[Count]) {
  const auto index = static_cast<int>(value);
  return active_source_quote(index >= 0 && static_cast<std::size_t>(index) < Count ? names[index] : "UNKNOWN");
}
template <typename Enum>
std::string enum_index(Enum value) { return scalar(static_cast<int>(value)); }

std::string slots(const std::vector<minera::BusinessHourSlot>& values) {
  return array(values, [](const auto& slot) {
    return object({{"start_seconds", scalar(slot.start_seconds)}, {"end_seconds", scalar(slot.end_seconds)}});
  });
}

std::string any(const std::any& value) {
  if (!value.has_value()) return object({{"has_value", "false"}, {"type_name", "null"}, {"value", "null"}});
  std::string payload = "null", error = "null";
  if (value.type() == typeid(std::string)) payload = scalar(std::any_cast<const std::string&>(value));
  else if (value.type() == typeid(bool)) payload = scalar(std::any_cast<bool>(value));
  else if (value.type() == typeid(double)) payload = scalar(std::any_cast<double>(value));
  else if (value.type() == typeid(float)) payload = scalar(static_cast<double>(std::any_cast<float>(value)));
  else if (value.type() == typeid(int)) payload = scalar(std::any_cast<int>(value));
  else if (value.type() == typeid(std::int64_t)) payload = scalar(std::any_cast<std::int64_t>(value));
  else if (value.type() == typeid(std::uint64_t)) payload = scalar(std::any_cast<std::uint64_t>(value));
  else error = active_source_quote("This programmatic std::any type has no application JSON carrier.");
  return object({{"has_value", "true"}, {"type_name", active_source_quote(value.type().name())},
                 {"value", payload}, {"value_error", error}});
}

std::string alpha(const minera::AlphaMinerOptions& o) {
  return object({{"short_loops", scalar(o.short_loops)}, {"activity_attribute", scalar(o.activity_attribute)},
    {"remove_unconnected", scalar(o.remove_unconnected)}, {"case_id_attribute", scalar(o.case_id_attribute)},
    {"timestamp_attribute", scalar(o.timestamp_attribute)}, {"max_rows_to_materialize", optional(o.max_rows_to_materialize)},
    {"start_timestamp_attribute", scalar(o.start_timestamp_attribute)}, {"sort_caseid_required", scalar(o.sort_caseid_required)},
    {"sort_timestamp_along_case_id", scalar(o.sort_timestamp_along_case_id)}});
}

std::string heuristic(const minera::HeuristicMinerOptions& o) {
  constexpr const char* variants[] = {"CLASSIC", "PLUSPLUS"};
  constexpr const char* decorations[] = {"FREQUENCY", "PERFORMANCE"};
  constexpr const char* aggregations[] = {"MEAN", "MEDIAN", "MINIMUM", "MAXIMUM", "STANDARD_DEVIATION", "SUM", "RAW_VALUES", "ALL"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"activity_attribute", scalar(o.activity_attribute)}, {"timestamp_attribute", scalar(o.timestamp_attribute)},
    {"dependency_threshold", optional(o.dependency_threshold)}, {"min_frequency", scalar(o.min_frequency)},
    {"connect_all", scalar(o.connect_all)}, {"and_threshold", scalar(o.and_threshold)},
    {"min_activity_count", scalar(o.min_activity_count)}, {"decoration", enum_name(o.decoration, decorations)},
    {"decoration_index", enum_index(o.decoration)}, {"performance_aggregation", enum_name(o.performance_aggregation, aggregations)},
    {"performance_aggregation_index", enum_index(o.performance_aggregation)},
    {"dfg_pre_cleaning_noise_threshold", scalar(o.dfg_pre_cleaning_noise_threshold)},
    {"loop_length_two_threshold", scalar(o.loop_length_two_threshold)}, {"keep_once_per_case", scalar(o.keep_once_per_case)},
    {"most_common_paths", array(o.most_common_paths, [](const auto& path) {return "[" + scalar(path.first) + ',' + scalar(path.second) + ']';})},
    {"start_timestamp_attribute", optional(o.start_timestamp_attribute)}, {"lifecycle_instance_attribute", scalar(o.lifecycle_instance_attribute)},
    {"use_business_hours", scalar(o.use_business_hours)}, {"business_hour_slots", slots(o.business_hour_slots)},
    {"business_hour_slots_explicit", scalar(o.business_hour_slots_explicit)}, {"case_id_attribute", scalar(o.case_id_attribute)},
    {"plusplus_decoration_selector", optional(o.plusplus_decoration_selector)},
    {"plusplus_min_activity_count", optional(o.plusplus_min_activity_count)}, {"plusplus_min_frequency", optional(o.plusplus_min_frequency)},
    {"plusplus_decoration_value", optional(o.plusplus_decoration_value, [](const auto& value) {return minera::serialize_workflow_structured_value_json(value);})}});
}

std::string powl(const minera::PowlDiscoveryOptions& o) {
  constexpr const char* variants[] = {"TREE", "BRUTE_FORCE", "MAXIMAL", "DYNAMIC_CLUSTERING", "DYNAMIC_CLUSTERING_PLAIN"};
  constexpr const char* filters[] = {"DECREASING_FACTOR", "DYNAMIC"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"filtering_type", enum_name(o.filtering_type, filters)}, {"filtering_type_index", enum_index(o.filtering_type)},
    {"filtering_threshold", scalar(o.filtering_threshold)}, {"order_frequency_ratio", scalar(o.order_frequency_ratio)},
    {"brute_force_activity_limit", scalar(o.brute_force_activity_limit)}, {"activity_key", scalar(o.activity_key)},
    {"timestamp_key", scalar(o.timestamp_key)}, {"case_id_key", scalar(o.case_id_key)}});
}

std::string temporal(const minera::TemporalProfileOptions& o) {
  return object({{"activity_attribute", scalar(o.activity_attribute)}, {"case_id_attribute", scalar(o.case_id_attribute)},
    {"source_timestamp_attribute", scalar(o.source_timestamp_attribute)}, {"target_timestamp_attribute", scalar(o.target_timestamp_attribute)},
    {"source_completion_timestamp_attribute", scalar(o.source_completion_timestamp_attribute)}, {"target_start_timestamp_attribute", scalar(o.target_start_timestamp_attribute)},
    {"use_business_hours", scalar(o.use_business_hours)}, {"business_hour_slots", slots(o.business_hour_slots)},
    {"business_hour_slots_explicit", scalar(o.business_hour_slots_explicit)}, {"strict_attributes", scalar(o.strict_attributes)}, {"zeta", scalar(o.zeta)}});
}

std::string transition(const minera::TransitionSystemOptions& o) {
  constexpr const char* directions[] = {"FORWARD", "BACKWARD"};
  constexpr const char* views[] = {"SEQUENCE", "SET", "MULTISET"};
  return object({{"direction", enum_name(o.direction, directions)}, {"direction_index", enum_index(o.direction)},
    {"view", enum_name(o.view, views)}, {"view_index", enum_index(o.view)}, {"window", scalar(o.window)},
    {"include_data", scalar(o.include_data)}, {"activity_key", scalar(o.activity_key)}, {"case_id_key", scalar(o.case_id_key)},
    {"activity_attribute", scalar(o.activity_attribute)}, {"case_id_attribute", scalar(o.case_id_attribute)}});
}

std::string skeleton(const minera::LogSkeletonOptions& o) {
  return object({{"noise_threshold", scalar(o.noise_threshold)}, {"considered_constraints", array(o.considered_constraints)},
    {"activity_key", scalar(o.activity_key)}, {"considered_constraints_present", scalar(o.considered_constraints_present)},
    {"case_id_key", scalar(o.case_id_key)}});
}

std::string declare(const minera::DeclareDiscoveryOptions& o) {
  return object({{"considered_activities", optional(o.considered_activities, [](const auto& v) {return array(v);})},
    {"allowed_templates", optional(o.allowed_templates, [](const auto& v) {return array(v);})},
    {"min_support_ratio", scalar(o.min_support_ratio)}, {"min_confidence_ratio", scalar(o.min_confidence_ratio)},
    {"has_min_support_ratio", scalar(o.has_min_support_ratio)}, {"has_min_confidence_ratio", scalar(o.has_min_confidence_ratio)},
    {"auto_selection_multiplier", scalar(o.auto_selection_multiplier)}, {"activity_key", scalar(o.activity_key)}, {"case_id_key", scalar(o.case_id_key)}});
}
std::string declare_conformance(const minera::DeclareConformanceOptions& o) {
  return object({{"activity_key", scalar(o.activity_key)}, {"case_id_key", scalar(o.case_id_key)}});
}

std::string distances(const minera::MinimumSelfDistanceOptions& o) {
  constexpr const char* variants[] = {"LOG", "PANDAS", "POLARS"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"activity_key", scalar(o.activity_key)}, {"case_id_key", scalar(o.case_id_key)}});
}

std::string correlation(const minera::CorrelationMinerOptions& o) {
  constexpr const char* variants[] = {"CLASSIC", "CLASSIC_SPLIT", "TRACE_BASED"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"activity_attribute", scalar(o.activity_attribute)}, {"timestamp_attribute", scalar(o.timestamp_attribute)},
    {"start_timestamp_attribute", scalar(o.start_timestamp_attribute)}, {"case_id_attribute", scalar(o.case_id_attribute)},
    {"exact_time_matching", scalar(o.exact_time_matching)}, {"sample_size", scalar(o.sample_size)}});
}

std::string spectrum(const minera::PerformanceSpectrumOptions& o) {
  constexpr const char* variants[] = {"CONNECTED", "DISCONNECTED", "DATAFRAME", "LOG", "DATAFRAME_DISCONNECTED", "LOG_DISCONNECTED", "LAZYFRAME", "LAZYFRAME_DISCONNECTED"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"sample_size", scalar(o.sample_size)}, {"sort_log", scalar(o.sort_log)}, {"activity_attribute", scalar(o.activity_attribute)},
    {"timestamp_attribute", scalar(o.timestamp_attribute)}, {"case_id_attribute", scalar(o.case_id_attribute)}});
}

std::string batches(const minera::BatchDiscoveryOptions& o) {
  constexpr const char* variants[] = {"LOG", "PANDAS", "POLARS"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"merge_distance_seconds", scalar(o.merge_distance_seconds)}, {"min_batch_size", scalar(o.min_batch_size)},
    {"start_timestamp_attribute", scalar(o.start_timestamp_attribute)}, {"activity_attribute", scalar(o.activity_attribute)},
    {"resource_attribute", scalar(o.resource_attribute)}, {"timestamp_attribute", scalar(o.timestamp_attribute)},
    {"case_id_attribute", scalar(o.case_id_attribute)}, {"event_id_attribute", scalar(o.event_id_attribute)}});
}

std::string dfg(const minera::DfgStatisticalOptions& o) {
  constexpr const char* variants[] = {"NATIVE", "FREQUENCY_TRIPLES", "PERFORMANCE", "CASE_ATTRIBUTES", "CLEAN", "CLEAN_POLARS", "CLEAN_TIME"};
  constexpr const char* aggregations[] = {"MEAN", "MEDIAN", "MINIMUM", "MAXIMUM", "STANDARD_DEVIATION", "SUM", "RAW_VALUES", "ALL"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"window", scalar(o.window)}, {"keep_once_per_case", scalar(o.keep_once_per_case)},
    {"activity_attribute", scalar(o.activity_attribute)}, {"timestamp_attribute", scalar(o.timestamp_attribute)},
    {"start_timestamp_attribute", scalar(o.start_timestamp_attribute)}, {"performance_aggregation", enum_name(o.performance_aggregation, aggregations)},
    {"performance_aggregation_index", enum_index(o.performance_aggregation)}, {"use_business_hours", scalar(o.use_business_hours)},
    {"business_hour_slots", slots(o.business_hour_slots)}, {"business_hour_slots_explicit", scalar(o.business_hour_slots_explicit)},
    {"workcalendar", any(o.workcalendar)}, {"case_attributes", array(o.case_attributes)}, {"return_node_attributes", scalar(o.return_node_attributes)}});
}

std::string dfg_table(const minera::DfgTableOptions& o) {
  constexpr const char* measures[] = {"FREQUENCY", "PERFORMANCE", "BOTH", "COST"};
  constexpr const char* aggregations[] = {"MEAN", "MEDIAN", "MINIMUM", "MAXIMUM", "STANDARD_DEVIATION", "SUM", "RAW_VALUES", "ALL"};
  return object({{"measure", enum_name(o.measure, measures)}, {"measure_index", enum_index(o.measure)},
    {"activity_attribute", scalar(o.activity_attribute)}, {"target_activity_attribute", scalar(o.target_activity_attribute)},
    {"case_id_attribute", scalar(o.case_id_attribute)}, {"timestamp_attribute", scalar(o.timestamp_attribute)},
    {"start_timestamp_attribute", optional(o.start_timestamp_attribute)}, {"cost_attribute", optional(o.cost_attribute)},
    {"performance_aggregation", enum_name(o.performance_aggregation, aggregations)}, {"performance_aggregation_index", enum_index(o.performance_aggregation)},
    {"custom_aggregation", object({{"supplied", scalar(static_cast<bool>(o.custom_aggregation))},
       {"value_error", o.custom_aggregation ? active_source_quote("A native callable has no application JSON carrier.") : "null"}})},
    {"sort_caseid_required", scalar(o.sort_caseid_required)}, {"sort_timestamp_along_case_id", scalar(o.sort_timestamp_along_case_id)},
    {"window", scalar(o.window)}, {"keep_once_per_case", scalar(o.keep_once_per_case)}, {"reduce_columns", scalar(o.reduce_columns)},
    {"business_hours", scalar(o.business_hours)}, {"business_hour_slots", optional(o.business_hour_slots, [](const auto& v) {return slots(v);})},
    {"workcalendar", any(o.workcalendar)}, {"event_index_attribute", scalar(o.event_index_attribute)}, {"max_start_column", scalar(o.max_start_column)},
    {"min_complete_column", scalar(o.min_complete_column)}, {"overlap_column", scalar(o.overlap_column)}, {"strict", scalar(o.strict)},
    {"keep_first_following", scalar(o.keep_first_following)}});
}

std::string footprints(const minera::FootprintsOptions& o) {
  return object({{"discovery_variants", array(o.discovery_variants)}, {"model", scalar(o.model)},
    {"conformance_variants", array(o.conformance_variants)}, {"strict", scalar(o.strict)},
    {"enable_act_always_executed", scalar(o.enable_act_always_executed)}, {"activity_attribute", scalar(o.activity_attribute)},
    {"case_id_attribute", scalar(o.case_id_attribute)}, {"timestamp_attribute", scalar(o.timestamp_attribute)},
    {"start_timestamp_attribute", optional(o.start_timestamp_attribute)}, {"index_attribute", scalar(o.index_attribute)},
    {"sort_required", scalar(o.sort_required)}, {"initial_marking", optional(o.initial_marking, [](const auto& v) {return array(v);})}});
}

std::string hidden_paths(const minera::TokenReplayHiddenPathCache& cache) {
  if (const auto* paths = std::get_if<minera::TokenReplayHiddenPathMatrix>(&cache)) {
    std::map<std::size_t, std::map<std::size_t, std::vector<std::size_t>>> ordered;
    for (const auto& [source, row] : *paths) for (const auto& [target, path] : row) ordered[source][target] = path;
    // Explicit empty source rows remain distinct from omitted rows.
    for (const auto& [source, row] : *paths) { (void)row; ordered.try_emplace(source); }
    return object({{"kind", "\"indices\""}, {"rows", array(ordered, [](const auto& row) {
      return object({{"source", scalar(row.first)}, {"paths", array(row.second, [](const auto& entry) {
        return object({{"target", scalar(entry.first)}, {"transitions", array(entry.second)}});
      })}});
    })}});
  }
  if (const auto* paths = std::get_if<minera::TokenReplayNamedHiddenPathMatrix>(&cache)) {
    std::map<std::string, std::map<std::string, std::vector<std::string>>> ordered;
    for (const auto& [source, row] : *paths) {
      ordered.try_emplace(source);
      for (const auto& [target, path] : row) ordered[source][target] = path;
    }
    return object({{"kind", "\"names\""}, {"rows", array(ordered, [](const auto& row) {
      return object({{"source", scalar(row.first)}, {"paths", array(row.second, [](const auto& entry) {
        return object({{"target", scalar(entry.first)}, {"transitions", array(entry.second)}});
      })}});
    })}});
  }
  minera::EventLog attribute_carrier;
  attribute_carrier.attributes.push_back(std::get<minera::EventAttribute>(cache));
  return object({{"kind", "\"attribute\""},
    {"attribute_carrier", minera::serialize_active_source_log_json(attribute_carrier)}});
}

std::string token(const minera::TokenReplayOptions& o) {
  constexpr const char* variants[] = {"FORWARD", "BACKWARDS"};
  return object({{"variant", enum_name(o.variant, variants)}, {"variant_index", enum_index(o.variant)},
    {"stop_immediately_unfit", scalar(o.stop_immediately_unfit)}, {"activity_attribute", scalar(o.activity_attribute)},
    {"consider_remaining_in_fitness", scalar(o.consider_remaining_in_fitness)},
    {"try_to_reach_final_marking_through_hidden", scalar(o.try_to_reach_final_marking_through_hidden)},
    {"consider_activities_not_in_model_in_fitness", scalar(o.consider_activities_not_in_model_in_fitness)},
    {"initial_marking", array(o.initial_marking)}, {"final_marking", array(o.final_marking)},
    {"walk_through_hidden_transitions", scalar(o.walk_through_hidden_transitions)}, {"include_trace_diagnostics", scalar(o.include_trace_diagnostics)},
    {"case_id_attribute", scalar(o.case_id_attribute)}, {"max_rows_to_materialize", optional(o.max_rows_to_materialize)},
    {"include_duration_diagnostics", scalar(o.include_duration_diagnostics)}, {"timestamp_attribute", scalar(o.timestamp_attribute)},
    {"include_root_cause_diagnostics", scalar(o.include_root_cause_diagnostics)},
    {"root_cause", object({{"string_attributes", array(o.root_cause.string_attributes)}, {"numeric_attributes", array(o.root_cause.numeric_attributes)}, {"enable_multiplier", scalar(o.root_cause.enable_multiplier)}})},
    {"cleaning_token_flood", scalar(o.cleaning_token_flood)}, {"place_names", array(o.place_names)}, {"transition_names", array(o.transition_names)},
    {"initial_marking_keys", array(o.initial_marking_keys, [](bool v) {return scalar(v);})},
    {"final_marking_keys", array(o.final_marking_keys, [](bool v) {return scalar(v);})},
    {"exhaustive_invisible_exploration", scalar(o.exhaustive_invisible_exploration)},
    {"exact_initial_marking", optional(o.exact_initial_marking, [](const auto& v) {return array(v);})},
    {"exact_final_marking", optional(o.exact_final_marking, [](const auto& v) {return array(v);})},
    {"enable_pltr_fitness", scalar(o.enable_pltr_fitness)}, {"disable_variants", optional(o.disable_variants)}, {"return_names", scalar(o.return_names)},
    {"places_shortest_path_by_hidden", optional(o.places_shortest_path_by_hidden, [](const auto& v) {return hidden_paths(v);})},
    {"nullable_transition_names", optional(o.nullable_transition_names, [](const auto& v) {return array(v, [](const auto& name) {return optional(name);});})},
    {"transition_identity_hashes", optional(o.transition_identity_hashes, [](const auto& v) {return array(v);})},
    {"place_outgoing_transition_order", optional(o.place_outgoing_transition_order, [](const auto& v) {return array(v, [](const auto& row) {return array(row);});})},
    {"place_fitness_order", optional(o.place_fitness_order, [](const auto& v) {return array(v);})}});
}

std::string overlap(const minera::OverlapOptions& o) {
  return object({{"activity_attribute", scalar(o.activity_attribute)}, {"activity_key", scalar(o.activity_key)},
    {"start_timestamp_attribute", scalar(o.start_timestamp_attribute)}, {"completion_timestamp_attribute", scalar(o.completion_timestamp_attribute)},
    {"start_timestamp_source", scalar(o.start_timestamp_source)}, {"completion_timestamp_source", scalar(o.completion_timestamp_source)},
    {"timestamp_attribute", scalar(o.timestamp_attribute)}, {"timestamp_source", scalar(o.timestamp_source)}, {"timestamp_key", scalar(o.timestamp_key)},
    {"start_timestamp_key", scalar(o.start_timestamp_key)}, {"completion_timestamp_key", scalar(o.completion_timestamp_key)},
    {"epsilon", scalar(o.epsilon)}, {"strict", scalar(o.strict)}});
}

}  // namespace active_source_options_detail

std::string active_source_token_replay_options_json(const minera::TokenReplayOptions& options) {
  return active_source_options_detail::token(options);
}

std::string active_source_family_options_json(const minera::MineParams& p, std::string_view family) {
  using namespace active_source_options_detail;
  if (family == "alpha" || family == "alpha_log_preparation") return alpha(p.alpha);
  if (family == "heuristic" || family == "heuristic_petri_conversion") return heuristic(p.heuristic);
  if (family == "inductive" || family == "inductive_log_preparation" || family == "inductive_petri_conversion")
    return minera::serialize_inductive_options_json(p.inductive);
  if (family == "powl" || family == "powl_petri_conversion") return powl(p.powl);
  if (family == "temporal_profile" || family == "temporal_conformance") return temporal(p.temporal_profile);
  if (family == "transition_system") return transition(p.transition_system);
  if (family == "log_skeleton" || family == "log_skeleton_conformance") return skeleton(p.log_skeleton);
  if (family == "declare") return declare(p.declare_options);
  if (family == "declare_conformance") return declare_conformance(p.declare_conformance_options);
  if (family == "minimum_self_distance") return distances(p.minimum_self_distance);
  if (family == "correlation") {
    return object({{"shared_options", correlation(p.correlation)}, {"selected_variants", array(p.correlation_variants)},
      {"variant_options", array(p.correlation_variants, [&](const auto& name) {
        auto options = p.correlation; options.variant = correlation_variant(name);
        return object({{"name", scalar(name)}, {"options", correlation(options)}});
      })}});
  }
  if (family == "performance_spectrum" || family == "spectrum")
    return object({{"activities", array(p.spectrum_activities)}, {"options", spectrum(p.spectrum)}});
  if (family == "batches") return batches(p.batch_options);
  if (family == "dfg" || family == "dfg_variants") {
    return object({{"shared_options", dfg(p.dfg_statistics)}, {"statistical_variants", array(p.dfg_statistical_variants)},
      {"causal_variants", array(p.dfg_causal_variants)}, {"variant_options", array(p.dfg_statistical_variants, [&](const auto& name) {
        auto options = p.dfg_statistics; options.variant = dfg_statistical_variant(name);
        return object({{"name", scalar(name)}, {"options", dfg(options)}});
      })}});
  }
  if (family == "dfg_dataframe") {
    constexpr const char* backends[] = {"PANDAS", "POLARS"};
    constexpr const char* operations[] = {"GRAPH", "PARTIAL_ORDER", "CONCURRENT_EVENTS"};
    return object({{"backend", enum_name(p.dfg_backend, backends)}, {"backend_index", enum_index(p.dfg_backend)},
      {"operation", enum_name(p.dfg_operation, operations)}, {"operation_index", enum_index(p.dfg_operation)},
      {"requested", scalar(p.dfg_dataframe_requested)}, {"options", dfg_table(p.dfg_table_options)}});
  }
  if (family == "footprints") return footprints(p.footprints);
  if (family == "token_replay" || family.ends_with("_conformance")) {
    auto options = p.token_replay;
    if (!p.token_replay_activity_attribute_supplied) {
      if (family == "alpha_conformance") options.activity_attribute = p.alpha.activity_attribute;
      else if (family == "inductive_conformance") options.activity_attribute = p.inductive.activity_key;
      else if (family == "heuristic_conformance") options.activity_attribute = p.heuristic.activity_attribute;
      else if (family == "powl_conformance") options.activity_attribute = p.powl.activity_key;
    }
    return token(options);
  }
  if (family == "overlap") return overlap(p.overlap_options);
  if (family == "alpha_decoration") return object({{"activity_attribute", "\"concept:name\""}, {"timestamp_attribute", "\"time:timestamp\""}});
  if (family == "inductive_decoration") return object({{"activity_attribute", "\"concept:name\""}, {"timestamp_attribute", "\"time:timestamp\""}});
  if (family == "heuristic_decoration") return object({{"activity_attribute", scalar(p.heuristic.activity_attribute)}, {"timestamp_attribute", scalar(p.heuristic.timestamp_attribute)}});
  if (family == "powl_decoration") return object({{"activity_attribute", scalar(p.powl.activity_key)}, {"timestamp_attribute", scalar(p.powl.timestamp_key)}});
  // Native source conversion and fixed log-statistic functions have no
  // MineParams options. Their source preparation context is owned separately.
  return "null";
}
