#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include "minera/petri_net.hpp"
#include "minera/ocdfg_edge_performance_options.hpp"
#include "minera/token_integer.hpp"

namespace minera {

using OcelValue = std::variant<std::nullptr_t, bool, std::int64_t, double,
                               std::string>;

struct OcelAttribute {
  std::string key;
  OcelValue value;
};

struct OcelEvent {
  std::string id;
  std::string activity;
  std::string timestamp;
  std::vector<OcelAttribute> attributes;
};

struct OcelObject {
  std::string id;
  std::string type;
  std::vector<OcelAttribute> attributes;
};

struct OcelRelation {
  std::string event_id;
  std::string object_id;
  std::string qualifier;
};

struct OcelO2oRelation {
  std::string source_id;
  std::string target_id;
  std::string qualifier;
};

struct OcelE2eRelation {
  std::string source_id;
  std::string target_id;
  std::string qualifier;
};

struct OcelObjectChange {
  std::string object_id;
  std::string object_type;
  std::string timestamp;
  std::string field;
  OcelValue value;
};

// Declared OCEL type metadata is kept separately from observed records so
// empty types and attributes survive format round-trips.
using OcelTypeSchema = std::map<std::string, std::string>;
using OcelTypeCatalog = std::map<std::string, OcelTypeSchema>;

struct OcelTableColumns {
  // Declared table schemas survive empty tables and row filtering. Absent
  // declarations infer columns from the canonical fields and attributes.
  std::optional<std::vector<std::string>> events;
  std::optional<std::vector<std::string>> objects;
  std::optional<std::vector<std::string>> relations;
};

struct Ocel {
  std::vector<OcelEvent> events;
  std::vector<OcelObject> objects;
  std::vector<OcelRelation> relations;
  std::vector<OcelO2oRelation> o2o;
  std::vector<OcelE2eRelation> e2e;
  std::vector<OcelObjectChange> object_changes;
  std::map<std::string, OcelValue> globals;
  OcelTypeCatalog event_type_attributes;
  OcelTypeCatalog object_type_attributes;
  OcelTableColumns table_columns;
  // The reference events frame puts ocel:activity before ocel:timestamp
  // (read from OCEL 1.0 or 2.0 XML, or a CSV or SQLite table in that column
  // order); its OCEL 1.0 XML, CSV and SQLite writers follow the frame order.
  // Classic JSON carries it as "minera:eventActivityFirst".
  bool event_activity_first = false;
};

using OCEL = Ocel;

// One limit for every OCEL route and reader: it equals the server's request
// payload limit, so any OCEL a server route accepts or produces (an import
// envelope's log) is accepted by every other OCEL route.
inline constexpr std::size_t kOcelMaxDocumentBytes = 128u * 1024u * 1024u;

struct OcelJsonLimits {
  std::size_t max_bytes = kOcelMaxDocumentBytes;
  std::size_t max_depth = 64;
  std::size_t max_string_bytes = 1u * 1024u * 1024u;
  // A 120k-event classic JSON of 38 MB already has more than a million nodes.
  std::size_t max_nodes = 32u * 1024u * 1024u;
};

// "128 MiB" for whole mebibytes, otherwise "N bytes".
[[nodiscard]] std::string ocel_limit_text(std::size_t bytes);

// The first limit a JSON document breaks, as parse_ocel_json would report it,
// without building the document: nullopt when it fits. Checks the size,
// nesting depth, string length and node count only, not the syntax.
[[nodiscard]] std::optional<std::string> ocel_json_limit_violation(
    std::string_view json, const OcelJsonLimits& limits = {});

enum class OcelJsonFormat { kClassic, kStandard };

// How an importer reads event timestamps for its timestamp order.
enum class OcelTimestampReader {
  // OCEL 2.0 JSON and classic XML: ISO 8601 as Python 3.11 reads it. The UTC offset is validated and then dropped (the wall-clock fields
  // are kept as UTC), and microseconds compare.
  kIsoFormat,
  // Classic OCEL JSON: the ISO reading, but a timestamp it cannot read keeps
  // the file order of the whole log instead of raising, because native
  // adapters use the classic format as transport (empty or NaT timestamps).
  kIsoFormatOrFileOrder,
  // OCEL 2.0 XML: the ISO reading above (wall clock), or else the reference's
  // dateutil fallback, which keeps its offset: a fallback value with an offset
  // compares by its UTC instant, one without is naive (wall clock), and naive
  // next to aware values raises as the reference's sort does. Shapes outside
  // the native dateutil subset keep the file order of the whole log; a text
  // with no digit and no month or weekday word raises "String does not
  // contain a date".
  kDateutil,
  // SQLite: the UTC instant (offsets convert), compared at nanoseconds.
  kInstantNanoseconds,
};

// The reference importers order events by timestamp (ties keep file order)
// and relations by their event's new position (ties keep file order). The
// kIsoFormat reader throws std::invalid_argument on a timestamp it cannot
// read, as the reference importers raise, and kDateutil on one that is
// certainly no date; otherwise an unreadable timestamp keeps the file order
// of the whole log.
// Each readable event timestamp is also stored as the value the reference
// importer stores, so filters, features, statistics and exporters agree: the
// ISO wall clock stamped UTC (JSON, XML), or the UTC instant (SQLite, an aware
// dateutil fallback); a naive text is stamped UTC too, as the reference
// stores it, except a naive dateutil fallback, which stays naive. A text the
// native timestamp parser already reads as that UTC value with an offset of
// zero (Z, +00:00) is kept as is; a naive event time becomes
// "YYYY-MM-DDTHH:MM:SS[.ffffff]Z" (the reference JSON writers' spelling of an
// event time), any other text "YYYY-MM-DDTHH:MM:SS[.ffffff]+00:00". Object
// change times are read the same way (a naive one becomes "...+00:00").
void sort_ocel_events_by_timestamp(Ocel& log, OcelTimestampReader reader);
// `nanoseconds` selects kInstantNanoseconds, otherwise kIsoFormat.
void sort_ocel_events_by_timestamp(Ocel& log, bool nanoseconds = false);

// Converts the string values of attributes the type catalogs declare as
// dates (a type naming "date" or "time" in any case; for kDateutil, as
// read_ocel2_xml, one naming "date") to the UTC timestamp the reference
// reader stores, as pm4py.read_ocel2_json, read_ocel2_xml and read_ocel_xml
// do (read_ocel_json, read_ocel2_sqlite and read_ocel_csv keep the text;
// observed black-box). The ISO reading keeps the wall clock stamped UTC (an
// offset is dropped, a naive text is UTC). kIsoFormatOrFileOrder (OCEL 2.0
// JSON) keeps a text it cannot read; kIsoFormat (OCEL 1.0 XML) throws the
// reference's "Invalid isoformat string"; kDateutil (OCEL 2.0 XML) falls back
// to the dateutil subset (an aware value is its UTC instant, a naive one
// stays naive) and throws "String does not contain a date" for a text that
// certainly is none. An event or object value is spelled "...[.ffffff]Z", a
// change value "...+00:00" (the reference JSON writers' spellings), unless the
// text already is that UTC value. Non-string values are kept.
void convert_ocel_date_attributes(Ocel& log, OcelTimestampReader reader);

// One date value as convert_ocel_date_attributes converts it (`change`: an
// object-change value). kIsoFormatOrFileOrder (OCEL 2.0 JSON) falls back to
// the dateutil subset like kDateutil but keeps a text it cannot read, and
// stamps a naive change value UTC, as read_ocel2_json does. The OCEL 1.0 XML
// reader calls it for each <date>/<timestamp> element's value only, as
// read_ocel_xml converts per element (a <string> under the same key stays).
void convert_ocel_date_value(OcelValue& value, OcelTimestampReader reader, bool change = false);

// Keeps only the events and objects that take part in at least one
// event-object relation, as every reference OCEL JSON, XML and SQLite reader
// does after reading (the OCEL CSV reader keeps them). Relations to a dropped
// side, object-to-object links touching a dropped object, event-to-event links
// touching a dropped event and the changes of dropped objects go with them;
// a log without relations ends up with no events and no objects. Order,
// multiplicity and the type catalogs are kept.
void drop_unrelated_ocel_entries(Ocel& log);

// Reads OCEL 1.0 or 2.0 JSON as the reference readers do, including the
// drop of unrelated events and objects, unless the document carries
// "minera:keepUnrelated": true (see serialize_ocel_json).
Ocel parse_ocel_json(std::string_view json,
                     const OcelJsonLimits& limits = {});
// Exact native filter inputs may use literal empty IDs, type names and
// attribute names. The default interchange reader retains its validation.
Ocel parse_ocel_json_literal_names(std::string_view json,
                                  const OcelJsonLimits& limits = {});
// Writes events, objects, attributes and relations in log order. A log with
// events or objects outside every event-object relation gets a
// "minera:keepUnrelated": true member, and parse_ocel_json then keeps those
// entries rather than dropping them like the reference file readers (which
// ignore the member): OCELs travel between routes as this JSON. The classic
// transport also carries "minera:eventTimeOffset" and
// "minera:eventActivityFirst" (see Ocel::event_activity_first);
// `transport_markers` false leaves those two out, for a file a user keeps
// (the CLI's JSON output, the export download), which then reads as the
// reference's read_ocel_json reads it.
std::string serialize_ocel_json(const Ocel& log,
                                OcelJsonFormat format = OcelJsonFormat::kClassic,
                                bool transport_markers = true);

struct EtotEdge {
  std::string activity;
  std::string object_type;
  std::size_t frequency = 0;
};

struct EtotGraph {
  std::vector<std::string> activities;
  std::vector<std::string> object_types;
  std::vector<EtotEdge> edges;
};

EtotGraph discover_etot_classic(const Ocel& log);

struct OtgEdge {
  std::string source_type;
  std::string relation;
  std::string target_type;
  std::size_t frequency = 0;
};

struct OtgGraph {
  std::vector<std::string> object_types;
  std::vector<OtgEdge> edges;
};

OtgGraph discover_otg_classic(const Ocel& log);

[[nodiscard]] std::string etot_json(const EtotGraph& graph);
[[nodiscard]] std::string otg_json(const OtgGraph& graph);
[[nodiscard]] EtotGraph parse_etot_json(
    std::string_view json, const OcelJsonLimits& limits = {});
[[nodiscard]] OtgGraph parse_otg_json(
    std::string_view json, const OcelJsonLimits& limits = {});

struct OcdfgActivityMetric {
  std::vector<std::string> events;
  std::vector<std::string> unique_objects;
  // Legacy association spelling retained for existing consumers.
  std::vector<std::pair<std::string, std::string>> event_objects;
  // Explicit association pairs used by the object-centric metric contract.
  std::vector<std::pair<std::string, std::string>> total_objects;
};

struct OcdfgEdgeMetric {
  std::vector<std::pair<std::string, std::string>> event_couples;
  std::vector<std::string> unique_objects;
  // Legacy association spelling retained for existing consumers.
  std::vector<std::tuple<std::string, std::string, std::string>> event_objects;
  // Explicit event-couple/object triples used by the object-centric metric
  // contract.
  std::vector<std::tuple<std::string, std::string, std::string>> total_objects;
  // Legacy aggregate performance samples retained for compatibility.
  std::vector<double> performance;
  // Performance is exposed separately for event couples and total-object
  // triples.  The classic implementation currently yields the same samples
  // for both projections, but keeping them distinct preserves the API shape.
  std::vector<double> performance_event_couples;
  std::vector<double> performance_total_objects;
};

struct OcdfgGraph {
  std::vector<std::string> activities;
  std::vector<std::string> object_types;
  std::map<std::string, OcdfgActivityMetric> activities_indep;
  std::map<std::string, std::map<std::string, OcdfgActivityMetric>> activities_ot;
  std::map<std::string, std::map<std::string, OcdfgActivityMetric>> start_activities;
  std::map<std::string, std::map<std::string, OcdfgActivityMetric>> end_activities;
  std::map<std::string, std::map<std::pair<std::string, std::string>, OcdfgEdgeMetric>> edges;
  bool performance_computed = true;
};

struct OcdfgDiscoveryOptions {
  bool compute_edges_performance = true;
  // Optional selectors for the labels used by the discovery projection.
  // Empty selectors use the canonical activity and object type fields.
  std::string activity_attribute;
  std::string object_type_attribute;
  // Explicit options activate the faithful typed timing calculation. Omission
  // preserves the legacy discovery timestamp projection for existing callers.
  std::optional<OcdfgEdgePerformanceOptions> edge_performance_options;
};

OcdfgGraph discover_ocdfg_classic(const Ocel& log,
                                  bool compute_edges_performance = true);
OcdfgGraph discover_ocdfg_classic_with_options(
    const Ocel& log, const OcdfgDiscoveryOptions& options);
std::string serialize_ocdfg_json(const OcdfgGraph& graph);
[[nodiscard]] OcdfgGraph parse_ocdfg_json(
    std::string_view json, const OcelJsonLimits& limits = {});

struct EtotConformanceOptions {
  double alpha = 1.0;
  double beta = 1.0;
  double gamma = 1.0;
  double theta_relative = 0.1;
};

struct EtotConformanceResult {
  double fitness = 1.0;
  std::vector<std::string> missing_activities;
  std::vector<std::string> additional_activities;
  std::vector<std::string> missing_object_types;
  std::vector<std::string> additional_object_types;
  std::vector<std::pair<std::string, std::string>> missing_edges;
  std::vector<std::pair<std::string, std::string>> additional_edges;
  std::map<std::pair<std::string, std::string>, double> relative_frequency_delta;
};

EtotConformanceResult compare_etot(const EtotGraph& real,
                                   const EtotGraph& normative,
                                   const EtotConformanceOptions& options = {});
// The reference contract also accepts an OCEL as the real side and performs
// classic ETOT discovery internally.  Keep the graph overload above for
// callers that already materialized a projection.
EtotConformanceResult compare_etot(const Ocel& real,
                                   const EtotGraph& normative,
                                   const EtotConformanceOptions& options = {});
std::string etot_conformance_json(const EtotConformanceResult& result);

struct OtgConformanceOptions {
  double alpha = 1.0;
  double beta = 1.0;
  double gamma = 1.0;
  double theta = 0.2;
  std::map<std::string, double> theta_by_relation;
};

struct OtgConformanceResult {
  double fitness = 1.0;
  std::vector<std::string> missing_object_types;
  std::vector<std::string> additional_object_types;
  std::vector<std::tuple<std::string, std::string, std::string>> missing_edges;
  std::vector<std::tuple<std::string, std::string, std::string>> additional_edges;
  std::map<std::tuple<std::string, std::string, std::string>, double>
      non_conforming_edges;
};

OtgConformanceResult compare_otg(const OtgGraph& real,
                                  const OtgGraph& normative,
                                  const OtgConformanceOptions& options = {});
// Direct OCEL input follows the same deterministic classic OTG projection as
// the explicit graph path.
OtgConformanceResult compare_otg(const Ocel& real,
                                  const OtgGraph& normative,
                                  const OtgConformanceOptions& options = {});
std::string otg_conformance_json(const OtgConformanceResult& result);

struct OcdfgConformanceOptions {
  double theta_activity = 0.0;
  double theta_flow = 0.0;
  double alpha = 1.0;
  double beta = 1.0;
  double gamma = 1.0;
  double delta = 1.0;
  // Retained for source compatibility. Direct OCEL comparisons use canonical
  // labels. To compare a selected projection, discover it with explicit
  // options and pass that graph to the graph overload.
  std::string activity_attribute;
  std::string object_type_attribute;
};

struct OcdfgConformanceResult {
  double fitness = 1.0;
  std::vector<std::string> missing_activities;
  std::vector<std::string> additional_activities;
  std::vector<std::pair<std::string, std::string>> missing_flows;
  std::vector<std::pair<std::string, std::string>> additional_flows;
  std::map<std::string, std::int64_t> activity_measure_differences;
  std::vector<std::string> non_conforming_activities_in_measure;
  std::map<std::pair<std::string, std::string>, std::int64_t>
      flow_measure_differences;
  std::vector<std::pair<std::string, std::string>>
      non_conforming_flows_in_measure;
};

OcdfgConformanceResult compare_ocdfg(const OcdfgGraph& real,
                                     const OcdfgGraph& normative,
                                     const OcdfgConformanceOptions& options = {});
// Direct OCEL input uses the classic OCDFG projection.  Performance samples
// are not needed by the conformance metrics, so the adapter avoids computing
// them while preserving activity and event-couple measures.
OcdfgConformanceResult compare_ocdfg(
    const Ocel& real, const OcdfgGraph& normative,
    const OcdfgConformanceOptions& options = {});
std::string ocdfg_conformance_json(const OcdfgConformanceResult& result);

enum class OcpnInductiveVariant { kLog, kDfg };

struct OcpnDiscoveryOptions {
  double double_arc_threshold = 0.8;
  double noise_threshold = 0.0;
  OcpnInductiveVariant variant = OcpnInductiveVariant::kLog;
  // The classic form retains OCDFG activity/flow annotations.  The
  // no-annotation form discovers the same per-object-type nets without that
  // diagnostic payload.
  bool annotated = true;
  bool diagnostics_with_token_based_replay = false;
  // Optional selectors for the activity and object-type labels used by the
  // discovery projection. Empty selectors use OcelEvent::activity and
  // OcelObject::type. Non-empty selectors resolve scalar attributes on the
  // corresponding records and stringify them deterministically.
  std::string activity_attribute;
  std::string object_type_attribute;
  // Inductive-miner policy forwarded to each object-type projection. The
  // public discovery mines without fall-throughs or the strict sequence cut.
  bool disable_strict_sequence_cut = true;
  bool disable_fallthroughs = true;
  // The public discovery fails when an object type has no object related to
  // any event (the reference raises KeyError); SAW-net discovery keeps such
  // types with a net mined from their empty flattened log.
  bool require_related_object_types = true;
  // Mine each object's events in relation order instead of by time (SAW-net
  // discovery does; the DFG variant always does).
  bool mine_in_relation_order = false;
};

struct OcpnTokenReplayPlaceDiagnostics {
  TokenInteger missing = 0;
  TokenInteger remaining = 0;
  TokenInteger consumed = 0;
  TokenInteger produced = 0;
};

struct OcpnTokenReplayDiagnostics {
  bool enabled = false;
  std::uint64_t trace_count = 0;
  std::uint64_t fitting_traces = 0;
  std::vector<OcpnTokenReplayPlaceDiagnostics> places;
  std::vector<std::uint64_t> transition_activations;
};

struct OcpnGraph {
  std::vector<std::string> activities;
  std::vector<std::string> object_types;
  std::map<std::string, PetriNet> petri_nets;
  // Object-type slots participating in each activity.  Repeated entries
  // represent multiple objects of the same type bound to one event.
  std::map<std::string, std::vector<std::string>> activity_object_types;
  // Variable object-type slots permit any non-empty subset of currently
  // enabled objects of that type to participate in one activity binding.
  std::map<std::string, std::vector<std::string>>
      activity_variable_object_types;
  std::map<std::string, std::map<std::string, bool>> double_arcs_on_activity;
  // The annotated discovery metrics share the deterministic OCDFG shape.
  // Keeping them with the OCPN makes the annotation variant useful to native
  // callers without changing the compact Petri-net representation.
  OcdfgGraph annotations;
  bool annotated = true;
  std::map<std::string, OcpnTokenReplayDiagnostics> tbr_results;
};

OcpnGraph discover_ocpn(const Ocel& log,
                        const OcpnDiscoveryOptions& options = {});
std::string serialize_ocpn_json(const OcpnGraph& graph);

struct OcelDescriptionOptions {
  bool include_timestamps = true;
};

enum class OcelDescriptionVariant { kVariant1, kVariant2 };

[[nodiscard]] const char* ocel_description_variant_name(
    OcelDescriptionVariant variant);

struct OcelTupleEventDescription {
  std::string activity;
  std::vector<std::string> related_objects;
  std::string timestamp;
};

struct OcelTupleObjectDescription {
  std::string id;
  std::string type;
  std::string lifecycle_start;
  std::string lifecycle_end;
  double lifecycle_seconds = 0.0;
  bool has_lifecycle = false;
};

struct OcelTupleDescription {
  std::vector<OcelTupleEventDescription> events;
  std::vector<OcelTupleObjectDescription> objects;
  bool includes_timestamps = true;
};

OcelTupleDescription describe_ocel_tuples(
    const Ocel& log, const OcelDescriptionOptions& options = {});
std::string describe_ocel_text(const Ocel& log,
                               const OcelDescriptionOptions& options = {});
std::string serialize_ocel_description_json(const OcelTupleDescription& description);
std::string serialize_ocel_description_json(
    const Ocel& log, OcelDescriptionVariant variant,
    const OcelDescriptionOptions& options = {});

}  // namespace minera
