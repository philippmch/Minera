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
  // Round EZ: a text the reference frame keeps as a str although its type
  // declares the key a date. read_ocel_xml types each element (observed
  // black-box): a <date> becomes a datetime, a <string> (or any other tag)
  // under the same key and type stays its text, so the type's slice of the
  // column is object and every writer, filter and the flattening keep the
  // text where a date-declared timestamp text reads as a datetime (see
  // ocel_frame::reference_date_value). Both OCEL transports carry it as
  // "minera:textValues" (the keys of an event's or object's marked values).
  bool text = false;
};

// Round EL: pm4py's read_ocel2_sqlite keeps a time column as read_sql gave
// it when pd.to_datetime(utc=True, format="mixed") raises on it (observed
// black-box): Python str, int and float values and NaN for NULL. Each event
// or change time of such a column keeps its Python type here (the timestamp
// text holds the value: the text itself, the integer's digits, the float's
// repr, empty for NaN), so the routes and writers that meet it raise
// pm4py's type errors or write the value as pm4py does. kNone is a time of
// a converted column. The OCEL transport carries the kinds
// ("minera:rawEventTimes", "minera:rawChangeTimes").
// Round EN: a NULL time of a type table whose read_sql column is object
// (texts or blobs next to numbers, or NULLs only) is Python's None there, a
// NULL of a numeric or text-only table NaN (observed black-box: OC-DFG then
// names `NoneType`); kNull is such a None.
enum class OcelRawTime : unsigned char { kNone, kStr, kInt, kFloat, kNaN, kNull };

// The Python type name of a raw time ("str", "int", "float"; NaN is a float,
// None a NoneType).
[[nodiscard]] inline const char* ocel_raw_time_type(OcelRawTime kind) {
  return kind == OcelRawTime::kStr     ? "str"
         : kind == OcelRawTime::kInt   ? "int"
         : kind == OcelRawTime::kNull  ? "NoneType"
                                       : "float";
}

// Round EN: whether a raw time is missing for pandas (NaN or None).
[[nodiscard]] inline bool ocel_raw_time_missing(OcelRawTime kind) {
  return kind == OcelRawTime::kNaN || kind == OcelRawTime::kNull;
}

struct OcelEvent {
  std::string id;
  std::string activity;
  std::string timestamp;
  std::vector<OcelAttribute> attributes;
  OcelRawTime raw_time = OcelRawTime::kNone;
};

struct OcelObject {
  std::string id;
  std::string type;
  std::vector<OcelAttribute> attributes;
};

// Round EW: the Python type of a qualifier. read_ocel2_sqlite keeps a
// number of a relation table's qualifier column (a column with a numeric
// affinity) as an int or float, which its JSON writers write as a number
// and write_ocel2_xml refuses for an O2O link (observed black-box, probe
// ev q1/qprobe.py). The qualifier text is then the number's Python repr.
// Round EX: read_ocel2_json and read_ocel_json keep a JSON bool too (text
// "True" or "False"), which the JSON writers write as a bool.
enum class OcelQualifierKind : unsigned char { kStr, kInt, kFloat, kBool };

struct OcelRelation {
  std::string event_id;
  std::string object_id;
  std::string qualifier;
  OcelQualifierKind qualifier_kind = OcelQualifierKind::kStr;
};

struct OcelO2oRelation {
  std::string source_id;
  std::string target_id;
  std::string qualifier;
  OcelQualifierKind qualifier_kind = OcelQualifierKind::kStr;
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
  OcelRawTime raw_time = OcelRawTime::kNone;  // see OcelEvent
  // Round EN: the change row's other cells as pm4py's changes frame holds
  // them (observed black-box): an OCEL 2.0 SQLite change row's non-null
  // cells in other fields' columns, in table column order, then
  // read_ocel2_sqlite's "@@cumcount" (the row's place among its object's
  // rows over the object tables in type order). The writers that print
  // whole change rows print them (write_ocel2_sqlite, write_ocel_json), and
  // they type the changes frame's columns. The OCEL transport carries them
  // ("minera:cells").
  std::vector<OcelAttribute> cells;
};

// Round EN: read_ocel2_sqlite's row counter column of the changes frame.
inline constexpr const char* kOcelCumcountColumn = "@@cumcount";

// Round EM: pandas types a frame column over every row the frame was built
// from and keeps its columns in that build's order. The reference readers
// build their frames from every listed row and drop rows afterwards (events
// and objects without relations, empty names, the changes of dropped
// objects), and the writers drop rows from frames typed before (observed
// black-box: an unrelated object's text keeps a date column an object
// column). A native log keeps what those rows add per frame: the column
// order of the build and the dropped rows' value kinds per column, so the
// writers type and order the columns as pm4py does. The OCEL transport
// carries it as "minera:frameHints".
enum OcelFrameHintKind : unsigned char {
  kOcelHintBool = 1,
  kOcelHintInt = 2,
  kOcelHintFloat = 4,
  kOcelHintString = 8,
  kOcelHintNaiveDate = 16,
  kOcelHintAwareDate = 32,
  // Round EO: a type table's all-NULL column (read_sql's object None, which
  // keeps the concatenated column an object one; no value of its own).
  kOcelHintNone = 64,
};

struct OcelFrameHintColumn {
  std::string key;
  unsigned char kinds = 0;  // OcelFrameHintKind bits of the dropped values
  std::size_t present = 0;  // dropped rows holding a value (not null)
};

struct OcelFrameHint {
  // The frame's attribute columns in build order (empty: first appearance
  // over the kept rows).
  std::vector<std::string> columns;
  std::vector<OcelFrameHintColumn> dropped;
  std::size_t dropped_rows = 0;
  // Round EN (changes frame only): its "ocel:field" and "ocel:timestamp"
  // columns stand before the attribute column columns[field_at]
  // (columns[time_at]); unset, after every attribute column. pm4py's
  // write_ocel2_sqlite orders a type table's change-only columns so
  // (observed black-box).
  std::optional<std::size_t> field_at;
  std::optional<std::size_t> time_at;
  // Round EP (events and objects frames): the frame's own columns
  // (ocel:eid, ocel:timestamp, ocel:activity; ocel:oid, ocel:type) where
  // pandas holds them among the attribute columns, in frame order: each
  // before columns[at] (0: before every attribute column, kOcelHintEnd:
  // after every one). Empty: the default places before the attributes (id,
  // time and activity as Ocel::event_activity_first says; id and type).
  // Set from an OCEL CSV file's own column order, for read_ocel_csv's
  // objects frame without an object table (ocel:type, ocel:oid) and for
  // read_ocel2_sqlite's (ocel:oid, the attributes, ocel:type); the CSV
  // writers and the SQLite writer's type tables follow it (observed
  // black-box).
  std::vector<std::pair<std::string, std::size_t>> reserved;
  [[nodiscard]] bool empty() const {
    return columns.empty() && dropped.empty() && dropped_rows == 0 && !field_at && !time_at &&
           reserved.empty();
  }
};
inline constexpr std::size_t kOcelHintEnd = static_cast<std::size_t>(-1);

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
  // Whether the event and object global scopes are declared, even when they
  // hold nothing: an OCEL 2.0 JSON read declares both (empty), a classic JSON
  // read declares the scopes its file lists, every other reader none. The
  // OCEL 1.0 JSON and XML writers fill an undeclared scope with the
  // reference defaults (ocel:activity / ocel:type "__INVALID__").
  bool global_event_scope = false;
  bool global_object_scope = false;
  OcelTypeCatalog event_type_attributes;
  OcelTypeCatalog object_type_attributes;
  OcelTableColumns table_columns;
  // The reference events frame puts ocel:activity before ocel:timestamp
  // (read from OCEL 1.0 or 2.0 XML, or a CSV or SQLite table in that column
  // order); its OCEL 1.0 XML, CSV and SQLite writers follow the frame order.
  // Classic JSON carries it as "minera:eventActivityFirst".
  bool event_activity_first = false;
  // The reference relations frame is float64 (round DT): a JSON file whose
  // relations frame is empty (a relation-free file, which keeps no event and
  // no object either) reads so, and filters keep it. Its flattening and its
  // lifecycle enrichment then fail. An in-memory empty log (a filter result,
  // a relation-free SQLite read) keeps typed frames, so a log built in code
  // leaves this false; the OCEL transport carries it as
  // "minera:untypedRelations".
  bool untyped_relations = false;
  // The reference O2O frame is float64 when its reader found no link (round
  // EO, observed black-box): read_ocel2_sqlite on an empty object_object
  // table, read_ocel2_xml, read_ocel_json, read_ocel_xml and read_ocel_csv
  // without links, and read_ocel2_json when the file lists no link (one
  // dangling link makes it str); a frame whose links a filter removed keeps
  // its type. write_ocel2_sqlite then declares the
  // object_object columns REAL. The OCEL transport carries it as
  // "minera:untypedO2o" while the log has no link.
  bool untyped_o2o = false;
  // Round EY: the dtype of the reference relations and O2O qualifier
  // columns where the reader or a transform fixes it (kStr for an object or
  // str column; nullopt: the rows' kinds, see ocel_qualifier_dtype).
  // read_ocel2_json and read_ocel_json type a column from every row the
  // frame is built from, read_ocel2_sqlite from every table row (round EX,
  // EW); a frame whose rows the reader or a filter removed keeps it, so
  // write_ocel2_sqlite declares an emptied int64 or bool column INTEGER and
  // a float64 one REAL, and the activity-type matching adds its separator
  // to such a column with numpy's UFuncTypeError, rows or not; the E2O
  // lifecycle enrichment leaves a str column (float64 on a frame without
  // rows), the O2O enrichment's links make the O2O column str (observed
  // black-box, probes ey p/e1.py..e3b.py). The OCEL transport carries it as
  // "minera:qualifierDtypes" where it differs from the rows' kinds.
  std::optional<OcelQualifierKind> relation_qualifier_dtype;
  std::optional<OcelQualifierKind> o2o_qualifier_dtype;
  // The resolution of a naive reference events frame (round DU), empty for
  // an aware one: an OCEL CSV file whose times carry no offset reads as
  // datetime64[us] (ns when a time has more than six fraction digits, s
  // without rows), and both reference playouts build datetime64[s] frames.
  // filter_ocel_events_timestamp then raises TypeError; the writers already
  // print such a log's naive texts without offset. Filters, splits and
  // enrichment keep it; the OCEL transport carries it as
  // "minera:naiveEventTimes" (the texts as written).
  std::string naive_event_time_unit;
  // Round EH: an OCEL 1.0 XML file none of whose events has a timestamp
  // element reads in pm4py as an object column of None rather than NaT
  // (observed black-box), so the routes that use the times raise Python's
  // None errors (AttributeError, TypeError) where a NaT log raises
  // ValueError or measures 0. The times stay empty (NaT) natively; filters
  // and splits keep the flag, and the OCEL transport carries it as
  // "minera:noneEventTimes".
  bool none_event_times = false;
  // Round EM: the rows the reader or a writer dropped (see OcelFrameHint).
  OcelFrameHint event_frame_hint;
  OcelFrameHint object_frame_hint;
  OcelFrameHint change_frame_hint;
};

using OCEL = Ocel;

// Round EY: the dtype of a qualifier column (see
// Ocel::relation_qualifier_dtype): the fixed one, else the kind every row
// shares (kStr for no row or a mix).
template <class Rows>
[[nodiscard]] OcelQualifierKind ocel_qualifier_dtype(const Rows& rows,
                                                     const std::optional<OcelQualifierKind>& dtype) {
  if (dtype) return *dtype;
  if (rows.empty()) return OcelQualifierKind::kStr;
  const auto kind = rows.front().qualifier_kind;
  for (const auto& row : rows)
    if (row.qualifier_kind != kind) return OcelQualifierKind::kStr;
  return kind;
}

// numpy's name of a qualifier dtype: object, int64, float64 or bool.
[[nodiscard]] inline const char* ocel_qualifier_dtype_name(OcelQualifierKind kind) {
  switch (kind) {
    case OcelQualifierKind::kInt: return "int64";
    case OcelQualifierKind::kFloat: return "float64";
    case OcelQualifierKind::kBool: return "bool";
    case OcelQualifierKind::kStr: break;
  }
  return "object";
}

// Whether the log's event time column is raw (see OcelRawTime).
[[nodiscard]] inline bool ocel_raw_event_times(const Ocel& log) {
  for (const auto& event : log.events)
    if (event.raw_time != OcelRawTime::kNone) return true;
  return false;
}

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
  // parse_ocel_json raises read_ocel_json's error for such a time while it
  // walks the file, unless the document is a transport that says it holds
  // one ("minera:unreadableEventTimes", see serialize_ocel_json).
  kIsoFormatOrFileOrder,
  // OCEL 2.0 XML: the ISO reading above (wall clock), or else the reference's
  // dateutil fallback (dateutil.parser.parse, fuzzy, today's date for missing
  // fields), which keeps its offset: a fallback value with an offset
  // compares by its UTC instant, one without is naive (wall clock), and naive
  // next to aware values raises as the reference's sort does. dateutil's
  // errors are raised. Texts outside the native dateutil model (characters
  // beyond printable ASCII, integers past 18 digits, offsets of a day or
  // more) keep the file order of the whole log; such
  // a text with no digit and no month or weekday word raises "String does
  // not contain a date".
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

// pm4py.read_ocel2_sqlite's event order, as pandas reads the time column
// (`times[i]` is log.events[i]'s cell; a text one is the event's
// timestamp): a column of numbers sorts as numbers, NULL last (declared: the
// reference keeps the numbers, native their texts); an object column reads
// through pd.to_datetime(utc=True, format="mixed") (integers and floats as
// nanoseconds since the epoch, NULL and NaT spellings as NaT, which sorts
// last as an empty text), then orders as kInstantNanoseconds; one that raises
// there stays texts, which sort as Python sorts them. Outside that model
// (blobs, numbers among texts that raise, the shapes pandas_to_datetime
// leaves out) the order is kInstantNanoseconds' reading of the texts.
struct OcelSqliteTime {
  enum class Kind { kNull, kInteger, kReal, kText, kBlob };
  Kind kind = Kind::kNull;
  std::int64_t integer = 0;
  double real = 0;
  // Round EN: a NULL read_sql reads as None (its table's column is object:
  // texts or blobs next to numbers, blobs, or NULLs only), not NaN.
  bool none = false;
};
// pandas' dtype of the concatenated column.
enum class OcelSqliteTimeColumn { kInteger, kReal, kObject };
void sort_ocel2_sqlite_events(Ocel& log, const std::vector<OcelSqliteTime>& times, OcelSqliteTimeColumn column);

// Converts the string values of attributes the type catalogs declare as
// dates (a type naming "date" or "time" in any case; for kDateutil, as
// read_ocel2_xml, one naming "date") to the UTC timestamp the reference
// reader stores, as pm4py.read_ocel2_json, read_ocel2_xml and read_ocel_xml
// do (read_ocel_json, read_ocel2_sqlite and read_ocel_csv keep the text;
// observed black-box). The ISO reading keeps the wall clock stamped UTC (an
// offset is dropped, a naive text is UTC). kIsoFormatOrFileOrder (OCEL 2.0
// JSON) keeps a text it cannot read; kIsoFormat (OCEL 1.0 XML) throws the
// reference's "Invalid isoformat string"; kDateutil (OCEL 2.0 XML) falls back
// to fuzzy dateutil (an aware value is its UTC instant, a naive one stays
// naive) and throws its errors. An event or object value is spelled "...[.ffffff]Z", a
// change value "...+00:00" (the reference JSON writers' spellings), unless the
// text already is that UTC value. Non-string values are kept.
void convert_ocel_date_attributes(Ocel& log, OcelTimestampReader reader);

// One date value as convert_ocel_date_attributes converts it (`change`: an
// object-change value). kIsoFormatOrFileOrder (OCEL 2.0 JSON) falls back to
// dateutil without fuzzy reading and keeps a text it cannot read, and stamps
// a naive change value UTC (an aware one is its UTC instant), as
// read_ocel2_json does. The OCEL 1.0 XML
// reader calls it for each <date>/<timestamp> element's value only, as
// read_ocel_xml converts per element (a <string> under the same key stays).
void convert_ocel_date_value(OcelValue& value, OcelTimestampReader reader, bool change = false);

// read_ocel2_xml reads each time and declared date value as it walks the
// file, so the first text that neither fromisoformat nor fuzzy dateutil reads
// raises, in document order (round EB, observed black-box). Throws that
// error for `text`.
void check_ocel2_xml_date_text(std::string_view text);

// Keeps only the events and objects that take part in at least one
// event-object relation, as every reference OCEL JSON, XML and SQLite reader
// does after reading (the OCEL CSV reader keeps them). Relations to a dropped
// side, object-to-object links touching a dropped object, event-to-event links
// touching a dropped event and the changes of dropped objects go with them;
// a log without relations ends up with no events and no objects. Order,
// multiplicity and the type catalogs are kept.
void drop_unrelated_ocel_entries(Ocel& log);
// Round EH: whether drop_unrelated_ocel_entries would change `log`.
bool ocel_has_unrelated_entries(const Ocel& log);
// Round EH: the log a file writer writes. Every reference writer (OCEL 1.0
// and 2.0 JSON and XML, SQLite, CSV) leaves out the events and objects
// without an event-object relation, with their O2O links and object
// changes (black-box), so the native writers for a user's file (the export
// downloads, the CLI) write `log` without them: `log` itself when it holds
// none, else a copy kept in `storage`. The classic JSON transport between
// routes keeps them (with "minera:keepUnrelated").
const Ocel& ocel_written_entries(const Ocel& log, std::optional<Ocel>& storage);
// Drops the events with an empty id or activity and the objects with an empty
// id or type, with the relations, object links, event links and changes that
// name them, and the empty type names of both catalogs: the reference OCEL
// 1.0/2.0 JSON, both XML and the SQLite readers drop such rows (observed
// black-box), then drop_unrelated_ocel_entries applies as usual.
void drop_empty_ocel_names(Ocel& log);
// The reference objects frame of a log declared without any object column
// (see OcelTableColumns; an OCEL JSON file that lists no object is read so,
// round DT) has no `column`: a reference function that reads it raises
// KeyError. Throws std::invalid_argument "KeyError: '<column>'".
void require_ocel_object_column(const Ocel& log, std::string_view column);
// Both columns at once, as a frame selection: "KeyError: \"None of
// [Index(['ocel:oid', 'ocel:type'], dtype='str')] are in the [columns]\""
// (the description and the SQLite writer).
void require_ocel_object_columns(const Ocel& log);

// Round DV: pm4py.read_ocel_csv builds the relations frame of a file whose
// rows name no object with only ocel:qualifier (float64), and both reference
// playouts that produce no event build events and objects frames without any
// column next to that relations frame (observed black-box). Ocel::table_columns
// declares such frames: relations as {"ocel:qualifier"}, events and objects
// empty. A reference function that reads a missing column raises KeyError.
// A declared relations frame is degenerate when it lists none of ocel:eid,
// ocel:oid, ocel:activity, ocel:type and ocel:timestamp (exact call models
// declare literal role names next to those).
[[nodiscard]] bool ocel_relations_frame_degenerate(const Ocel& log);
// Throws std::invalid_argument "KeyError: '<column>'" when the relations frame
// is degenerate and lacks `column`.
void require_ocel_relation_column(const Ocel& log, std::string_view column);
// An events frame declared without any column (an empty playout).
[[nodiscard]] bool ocel_events_frame_columnless(const Ocel& log);
// Throws std::invalid_argument "KeyError: '<column>'" for such a frame.
void require_ocel_event_column(const Ocel& log, std::string_view column);
// Declares the frames above for a log without relations: the relations frame
// always, the events and objects frames too when `columnless_entities` (a
// playout) and the log holds no event and no object. A log with relations is
// left as it is.
void mark_ocel_relation_free_frames(Ocel& log, bool columnless_entities);

// True for a global the reference OCEL 1.0 writers produce by themselves: the
// "__INVALID__" defaults of the event and object scopes and the computed log
// scope (version, ordering, attribute names, object types). Writers that have
// no global scopes drop these without loss.
bool ocel_reference_generated_global(std::string_view key, const OcelValue& value);

// Reads OCEL 1.0 or 2.0 JSON as the reference readers do, including the
// drop of unrelated events and objects, unless the document carries
// "minera:keepUnrelated": true (see serialize_ocel_json). The classic form
// follows read_ocel_json's walk, values and errors (round EE, see
// parse_classic_reference in ocel.cpp).
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
// "minera:eventActivityFirst" (see Ocel::event_activity_first), and
// "minera:unreadableEventTimes" for a log holding an event time
// read_ocel_json cannot read (an empty, NaT, text or float time);
// `transport_markers` false leaves those three out, for a file a user keeps
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
  // Each object's trace is its relation rows (repeats kept) in event order
  // (round BZ); the classic miner sorts them by time unless this is set
  // (SAW-net discovery mines them as listed). The IMd edges and the replay
  // diagnostics read the event order, the IMd start and end activities the
  // relation rows' order.
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
