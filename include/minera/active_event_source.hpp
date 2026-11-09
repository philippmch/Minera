#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/batches.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/declare.hpp"
#include "minera/event_data.hpp"
#include "minera/log_conversion.hpp"
#include "minera/log_filter.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/minimum_self_distance.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/transition_system.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

struct ActiveSourceFile {
  std::string name;
  std::string format;
  std::string bytes;
};

struct ActiveSourceSelectors {
  std::string case_id_key = "case:concept:name";
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  std::string resource_key = "org:resource";
  std::string lifecycle_key = "lifecycle:transition";
};

// Changes describe the integration projection into source-declared columns;
// they do not assert that native filtering has an EventTable overload.
struct ActiveSourceCellChange {
  std::size_t checked_row = 0;
  std::size_t source_row = 0;
  std::string column;
  bool removed = false;
};

// legacy_view keeps the established conversion presentation. native_state
// retains raw stored fields, enum indexes and exact typed attribute carriers.
[[nodiscard]] std::string serialize_active_source_log_json(const EventLog& log);
// The same native_state for a generated (playout) log, without legacy_view
// (null) and with the repeated parts factored out: "defaults" holds every
// trace and event field of the first trace and event, "activity_table" each
// distinct activity once, and each trace and event writes only the fields that
// differ from those defaults; an event refers to its activity by
// "activity_ref" and omits a case_id equal to its trace's. Expanding an event
// takes the keys of defaults.event in order. See expandPlayoutEvents in the
// frontend's playout-workbench.ts.
[[nodiscard]] std::string serialize_active_source_log_compact_json(const EventLog& log);
[[nodiscard]] std::string serialize_active_source_stream_json(const EventStream& stream);
[[nodiscard]] std::string serialize_active_source_skeleton_json(
    const LogSkeletonModel& model,
    const std::vector<LogSkeletonConformance>* conformance = nullptr);
[[nodiscard]] std::string serialize_active_source_declare_json(
    const DeclareModel& model,
    const std::vector<DeclareConformance>* conformance = nullptr);
[[nodiscard]] std::string serialize_active_source_transition_json(const TransitionSystem& result);
[[nodiscard]] std::string serialize_active_source_distances_json(const MinimumSelfDistances& result);
[[nodiscard]] std::string serialize_active_source_correlation_json(const CorrelationVariantsAnalysis& result);
[[nodiscard]] std::string serialize_active_source_spectrum_json(const PerformanceSpectrum& result);
[[nodiscard]] std::string serialize_active_source_batches_json(const BatchDiscovery& result);
[[nodiscard]] std::string serialize_active_source_temporal_profile_json(const TemporalProfile& result);
[[nodiscard]] std::string serialize_active_source_temporal_conformance_json(const TemporalConformanceResult& result);

// All referenced views are owned by this value. Copies share immutable
// storage, while filters return a new value and mutable consumers get copies.
class ActiveEventSource {
 public:
  using NativeFilterPipeline = std::function<EventLog(const EventLog&)>;
  using RepresentativeRows =
      std::vector<std::vector<std::optional<std::size_t>>>;

  [[nodiscard]] static ActiveEventSource from_table(
      EventTable table, ActiveSourceFile file,
      ActiveSourceSelectors selectors = {},
      EventDataKind selected = EventDataKind::kEventTable);
  // `cancelled` (from_csv, from_xes_frame) is polled while the source's
  // projections are built; once it answers true the build throws
  // OperationCancelled. Unset: never cancels, and the source is unchanged.
  [[nodiscard]] static ActiveEventSource from_csv(
      CsvEventData data, ActiveSourceFile file,
      ActiveSourceSelectors selectors = {},
      EventDataKind selected = EventDataKind::kEventLog,
      const std::function<bool()>& cancelled = {});
  [[nodiscard]] static ActiveEventSource from_log(
      EventLog log, ActiveSourceFile file,
      ActiveSourceSelectors selectors = {},
      EventDataKind selected = EventDataKind::kEventLog);
  // An imported XES log on pm4py's DataFrame path (pm4py.read_xes): the
  // source's log is merge_dataframe_cases of `frame`, and its table holds the
  // frame's rows: every event in file order with its own trace's cells. A
  // filtered source keeps projecting those rows.
  [[nodiscard]] static ActiveEventSource from_xes_frame(
      EventLog frame, ActiveSourceFile file,
      ActiveSourceSelectors selectors = {},
      EventDataKind selected = EventDataKind::kEventLog,
      const std::function<bool()>& cancelled = {});

  [[nodiscard]] const ActiveSourceFile& file() const noexcept;
  [[nodiscard]] const ActiveSourceSelectors& selectors() const noexcept;
  [[nodiscard]] EventDataKind selected_kind() const noexcept;
  [[nodiscard]] const LogConversionOptions& preparation_options() const noexcept;
  [[nodiscard]] const std::string& original_table_origin() const noexcept;
  [[nodiscard]] const std::string& checked_table_origin() const noexcept;

  [[nodiscard]] const EventTable& original_table() const;
  [[nodiscard]] const EventLog& original_log() const;
  [[nodiscard]] const EventTable& checked_table() const;
  [[nodiscard]] const EventLog& checked_log() const;
  [[nodiscard]] const EventStream& checked_stream() const;
  [[nodiscard]] const LazyEventTable& checked_lazy() const;
  [[nodiscard]] const EventTable* original_table_if_available() const noexcept;
  [[nodiscard]] const EventLog* original_log_if_available() const noexcept;
  [[nodiscard]] const EventTable* checked_table_if_available() const noexcept;
  [[nodiscard]] const EventLog* checked_log_if_available() const noexcept;
  [[nodiscard]] const EventStream* checked_stream_if_available() const noexcept;
  [[nodiscard]] const LazyEventTable* checked_lazy_if_available() const noexcept;

  [[nodiscard]] const std::string& checked_log_preparation_error() const noexcept;
  [[nodiscard]] const std::string& checked_flat_projection_error() const noexcept;
  [[nodiscard]] const std::vector<std::size_t>& source_row_ordinals() const noexcept;
  [[nodiscard]] const RepresentativeRows& representative_row_ordinals() const noexcept;
  [[nodiscard]] const std::vector<ActiveSourceCellChange>& projection_changes() const noexcept;

  // A returned view must not outlive this source. No unavailable shape is
  // silently replaced with a different representation.
  [[nodiscard]] EventDataView view(EventDataKind kind) const;
  [[nodiscard]] EventDataView selected_view() const;
  [[nodiscard]] EventTable mutable_table_copy() const;
  [[nodiscard]] ActiveEventSource with_filters(const LogFilterOptions& options) const;
  // The caller lists every attribute key read by a composed native pipeline.
  // Its private provenance field is chosen outside that set and the source.
  [[nodiscard]] ActiveEventSource with_filters(
      const NativeFilterPipeline& pipeline,
      const std::vector<std::string>& referenced_attribute_keys = {}) const;

 private:
  struct State;
  explicit ActiveEventSource(std::shared_ptr<const State> state);
  std::shared_ptr<const State> state_;
};

}  // namespace minera
