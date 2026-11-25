#pragma once

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "minera/powl.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace minera {

// Clean-room implementations of the public POWL discovery variants.
// kDynamicClustering is the frequency-based dynamic clustering variant;
// kDynamicClusteringPlain exposes the otherwise-unreachable unweighted
// implementation so callers can select every source variation explicitly.
enum class PowlDiscoveryVariant {
  kTree,
  kBruteForce,
  kMaximal,
  kDynamicClustering,
  kDynamicClusteringPlain,
  TREE = kTree,
  BRUTE_FORCE = kBruteForce,
  MAXIMAL = kMaximal,
  DYNAMIC_CLUSTERING = kDynamicClustering,
  DYNAMIC_CLUSTERING_PLAIN = kDynamicClusteringPlain,
};

enum class PowlFilteringType {
  kDecreasingFactor,
  kDynamic,
  DECREASING_FACTOR = kDecreasingFactor,
  DYNAMIC = kDynamic,
};

struct PowlDiscoveryOptions {
  PowlDiscoveryVariant variant = PowlDiscoveryVariant::kMaximal;
  PowlFilteringType filtering_type = PowlFilteringType::kDecreasingFactor;
  double filtering_threshold = 0.0;
  double order_frequency_ratio = 1.0;
  // Zero means unlimited. A positive value fails before brute-force
  // partition enumeration when the local alphabet is larger than this cap.
  std::size_t brute_force_activity_limit = 0;
  // Event field used to form POWL activity labels. The native activity field
  // is selected by the canonical concept:name spelling; other names resolve
  // through first-class fields or Event::attribute and must be present on
  // every event.
  std::string activity_key = "concept:name";
  // Timestamp and case selectors are applied while adapting EventLog inputs.
  // A custom timestamp orders events stably; a custom case field regroups
  // flattened event rows into traces.
  std::string timestamp_key = "time:timestamp";
  std::string case_id_key = "case:concept:name";
};

// Applies the selectors used by POWL to a native event log. Custom case
// selectors regroup event rows, custom timestamp selectors stably order each
// case, and custom activity selectors replace the visible activity label.
// Canonical defaults retain the existing EventLog trace and event order.
[[nodiscard]] EventLog normalize_powl_event_log(
    const EventLog& event_log, const PowlDiscoveryOptions& options = {});

[[nodiscard]] PowlNode discover_powl(
    const EventLog& event_log, const PowlDiscoveryOptions& options = {});

// Whether POWL discovery on `event_log` (an upload's log shape) reads the
// activity key's objects rather than its text (round HK): the discovered
// net's labels then name those objects, and normalize_powl_event_log holds
// the same names under concept:name (a replay keyed the same reads them
// there). Throws the discovery's KeyError or TypeError.
[[nodiscard]] bool powl_keyed_by_log_objects(const EventLog& event_log, const PowlDiscoveryOptions& options);
// The net names of the key's falsy objects (0, 0.0, -0.0, False, '') when
// the discovery reads them (powl_keyed_by_log_objects): pm4py's stochastic
// map times a transition so labelled as a hidden one (round HM).
[[nodiscard]] std::vector<std::string> powl_falsy_labels(const EventLog& event_log, const PowlDiscoveryOptions& options);
// The same discovery; `cancelled`, when set, is polled at every recursion
// step and fall-through candidate and throws OperationCancelled
// (minera/cancellation.hpp) once it returns true. The fall-throughs try every
// activity of a sublog in turn, so logs with hundreds of activities can take
// many minutes.
[[nodiscard]] PowlNode discover_powl(
    const EventLog& event_log, const PowlDiscoveryOptions& options,
    const std::function<bool()>& cancelled);

// The same discovery on an upload's frame, as pm4py's POWL reads a pandas or
// Polars frame: each case's activities after the frame is sorted by case and
// timestamp (pandas stably, Polars with its unstable pdqsort), the
// Inductive miner's frame projection (prepare_inductive_event_log). An
// EventLog view (and an event stream, which pm4py's POWL does not read as a
// frame) uses the log overloads above. Round FV, observed black-box.
[[nodiscard]] PowlNode discover_powl(
    const EventDataView& data, const PowlDiscoveryOptions& options,
    const std::function<bool()>& cancelled = {});
[[nodiscard]] EventLog normalize_powl_event_log(
    const EventDataView& data, const PowlDiscoveryOptions& options = {});

// The same on the frame of an XES upload, gaps and missing columns as
// pm4py's project_univariate reads them (prepare_inductive_event_log_xes_frame;
// round FY).
[[nodiscard]] PowlNode discover_powl_xes_frame(
    const EventDataView& data, const PowlDiscoveryOptions& options,
    const std::function<bool()>& cancelled = {});
[[nodiscard]] EventLog normalize_powl_event_log_xes_frame(
    const EventDataView& data, const PowlDiscoveryOptions& options = {});

}  // namespace minera
