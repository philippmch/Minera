#include "discovery_registry.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace minera::footprints_detail {
namespace {

using Selector = FootprintsDiscoverySelector;

const char* selector_name(Selector selector) noexcept {
  switch (selector) {
    case Selector::kEntireEventLog: return "ENTIRE_EVENT_LOG";
    case Selector::kEntireDataframe: return "ENTIRE_DATAFRAME";
    case Selector::kTraceByTrace: return "TRACE_BY_TRACE";
    case Selector::kPetriReachGraph: return "PETRI_REACH_GRAPH";
    case Selector::kProcessTree: return "PROCESS_TREE";
    case Selector::kPowl: return "POWL";
    case Selector::kDfg: return "DFG";
    case Selector::kPolarsLazyframes: return "POLARS_LAZYFRAMES";
  }
  return "";
}

std::invalid_argument wrong_input(Selector selector, std::string_view input) {
  return std::invalid_argument("footprints selector " +
                               std::string(selector_name(selector)) +
                               " cannot consume " + std::string(input));
}

bool uses_default_table_parameters(const DiscoveryParameters& parameters) {
  return parameters.activity_key == "concept:name" &&
         parameters.case_id_key == "case:concept:name" &&
         parameters.timestamp_key == "time:timestamp" &&
         !parameters.start_timestamp_key && parameters.index_key == "@@index" &&
         parameters.sort_required;
}

void reject_ignored_table_parameters(
    Selector selector, const DiscoveryParameters& parameters,
    std::string_view input) {
  if (uses_default_table_parameters(parameters)) return;
  throw std::invalid_argument(
      "footprints selector " + std::string(selector_name(selector)) +
      " cannot ignore custom table selectors while converting " +
      std::string(input));
}

}  // namespace

const std::vector<FootprintsDiscoverySelectorDescriptor>&
FootprintsDiscoverySelectorRegistry::all() {
  static const std::vector<FootprintsDiscoverySelectorDescriptor> descriptors =
      {{Selector::kEntireEventLog, "ENTIRE_EVENT_LOG"},
       {Selector::kEntireDataframe, "ENTIRE_DATAFRAME"},
       {Selector::kTraceByTrace, "TRACE_BY_TRACE"},
       {Selector::kPetriReachGraph, "PETRI_REACH_GRAPH"},
       {Selector::kProcessTree, "PROCESS_TREE"},
       {Selector::kPowl, "POWL"},
       {Selector::kDfg, "DFG"},
       {Selector::kPolarsLazyframes, "POLARS_LAZYFRAMES"}};
  return descriptors;
}

bool FootprintsDiscoverySelectorRegistry::supports(Selector selector) noexcept {
  return std::any_of(all().begin(), all().end(),
                     [selector](const auto& descriptor) {
                       return descriptor.selector == selector;
                     });
}

std::optional<Selector> FootprintsDiscoverySelectorRegistry::parse(
    std::string_view name) {
  for (const auto& descriptor : all())
    if (descriptor.name == name) return descriptor.selector;
  return std::nullopt;
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const EventLog& log, Selector selector, const DiscoveryParameters& parameters) {
  switch (selector) {
    case Selector::kEntireEventLog:
      return EntireEventLogFootprintsDiscovery::apply(log, parameters);
    case Selector::kEntireDataframe:
      reject_ignored_table_parameters(selector, parameters, "an event log");
      return EntireDataframeFootprintsDiscovery::apply(
          event_log_to_table(log), parameters);
    case Selector::kPolarsLazyframes:
      reject_ignored_table_parameters(selector, parameters, "an event log");
      return PolarsLazyframesFootprintsDiscovery::apply(
          event_log_to_lazy_table(log), parameters);
    case Selector::kTraceByTrace:
      return TraceByTraceFootprintsDiscovery::apply(log, parameters);
    case Selector::kDfg:
      return DfgFootprintsDiscovery::apply(discover_directly_follows_graph(log));
    case Selector::kPetriReachGraph:
    case Selector::kProcessTree:
    case Selector::kPowl:
      throw wrong_input(selector, "an event log");
  }
  throw std::invalid_argument("unsupported footprints selector");
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const EventTable& table, Selector selector,
    const DiscoveryParameters& parameters) {
  switch (selector) {
    case Selector::kEntireDataframe:
      return EntireDataframeFootprintsDiscovery::apply(table, parameters);
    case Selector::kPolarsLazyframes:
      return PolarsLazyframesFootprintsDiscovery::apply(
          LazyEventTable::from_table(table), parameters);
    case Selector::kEntireEventLog:
    case Selector::kTraceByTrace:
    case Selector::kDfg:
      // The generic table-to-log conversion has fixed source spellings and
      // cannot propagate this adapter's selectors.  Reject the cross-input
      // call instead of silently dropping caller options.
      reject_ignored_table_parameters(selector, parameters, "an event table");
      {
        const auto log = event_table_to_event_log(table);
        if (selector == Selector::kEntireEventLog)
          return EntireEventLogFootprintsDiscovery::apply(log, parameters);
        if (selector == Selector::kTraceByTrace)
          return TraceByTraceFootprintsDiscovery::apply(log, parameters);
        return DfgFootprintsDiscovery::apply(discover_directly_follows_graph(log));
      }
    case Selector::kPetriReachGraph:
    case Selector::kProcessTree:
    case Selector::kPowl:
      throw wrong_input(selector, "an event table");
  }
  throw std::invalid_argument("unsupported footprints selector");
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const LazyEventTable& table, Selector selector,
    const DiscoveryParameters& parameters) {
  switch (selector) {
    case Selector::kPolarsLazyframes:
      return PolarsLazyframesFootprintsDiscovery::apply(table, parameters);
    case Selector::kEntireDataframe:
      return EntireDataframeFootprintsDiscovery::apply(table.materialize(),
                                                       parameters);
    case Selector::kEntireEventLog:
    case Selector::kTraceByTrace:
    case Selector::kDfg:
      // See the eager table overload: conversion would otherwise discard
      // explicit activity/case/time ordering controls.
      reject_ignored_table_parameters(selector, parameters,
                                      "a lazy event table");
      {
        const auto log = lazy_event_table_to_event_log(table);
        if (selector == Selector::kEntireEventLog)
          return EntireEventLogFootprintsDiscovery::apply(log, parameters);
        if (selector == Selector::kTraceByTrace)
          return TraceByTraceFootprintsDiscovery::apply(log, parameters);
        return DfgFootprintsDiscovery::apply(discover_directly_follows_graph(log));
      }
    case Selector::kPetriReachGraph:
    case Selector::kProcessTree:
    case Selector::kPowl:
      throw wrong_input(selector, "a lazy event table");
  }
  throw std::invalid_argument("unsupported footprints selector");
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const DirectlyFollowsGraph& graph, Selector selector) {
  if (selector != Selector::kDfg) throw wrong_input(selector, "a directly-follows graph");
  return DfgFootprintsDiscovery::apply(graph);
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const std::map<FootprintPair, std::uint64_t>& dfg, Selector selector) {
  if (selector != Selector::kDfg) throw wrong_input(selector, "a DFG map");
  return DfgFootprintsDiscovery::apply(dfg);
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const ProcessTree& tree, Selector selector) {
  if (selector != Selector::kProcessTree)
    throw wrong_input(selector, "a process tree");
  return ProcessTreeBottomUpFootprintsDiscovery::apply(tree);
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const PowlNode& model, Selector selector) {
  if (selector != Selector::kPowl)
    throw wrong_input(selector, "a POWL model");
  return PowlBottomUpFootprintsDiscovery::apply(model);
}

FootprintsDiscoveryResult FootprintsDiscoverySelectorRegistry::dispatch(
    const PetriNet& net, Selector selector,
    const DiscoveryParameters& parameters) {
  if (selector != Selector::kPetriReachGraph)
    throw wrong_input(selector, "a Petri net");
  return PetriReachabilityFootprintsDiscovery::apply(net, parameters);
}

}  // namespace minera::footprints_detail
