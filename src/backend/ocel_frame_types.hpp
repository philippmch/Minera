#pragma once

// Shared helpers for the OCEL writers that reproduce what the reference
// writers emit from their pandas frames: the column dtype a frame built from
// the OCEL's typed values would get, Python's float and str reprs, and the
// text pandas prints for a UTC timestamp.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "minera/filtering_common.hpp"
#include "minera/ocel.hpp"
#include "minera/timestamp.hpp"

namespace minera::ocel_frame {

// The dtype pandas gives a column of a frame built from records: an integer
// column with a missing cell becomes float64, a boolean one object. A column
// of datetimes alone (missing cells are NaT) is datetime64, naive or aware;
// next to any other value, or naive next to aware, it is object (round EK).
enum class Dtype { kEmpty, kInt64, kFloat64, kBool, kObject, kDatetime };

// Whether `text` (a timestamp text) is a naive one.
inline bool naive_timestamp_text(std::string_view text) {
  const auto parsed = parse_timestamp_exact(text);
  return parsed && !parsed->timezone_aware;
}

struct ColumnStats {
  bool has_bool = false;
  bool has_int = false;
  bool has_float = false;
  bool has_string = false;
  // Round EK: date values (a date-declared text the reference reads as a
  // datetime), naive and aware apart.
  bool has_naive_date = false;
  bool has_aware_date = false;
  std::size_t present = 0;

  void observe(const OcelValue& value) { observe(value, false); }
  // `date`: the value is a datetime in the reference frame.
  void observe(const OcelValue& value, bool date) {
    if (std::holds_alternative<std::nullptr_t>(value)) return;
    ++present;
    if (std::holds_alternative<bool>(value)) has_bool = true;
    else if (std::holds_alternative<std::int64_t>(value)) has_int = true;
    else if (std::holds_alternative<double>(value)) has_float = true;
    else if (date) (naive_timestamp_text(std::get<std::string>(value)) ? has_naive_date : has_aware_date) = true;
    else has_string = true;
  }
  void merge(const ColumnStats& other) {
    has_bool = has_bool || other.has_bool;
    has_int = has_int || other.has_int;
    has_float = has_float || other.has_float;
    has_string = has_string || other.has_string;
    has_naive_date = has_naive_date || other.has_naive_date;
    has_aware_date = has_aware_date || other.has_aware_date;
    present += other.present;
  }
  [[nodiscard]] bool has_date() const { return has_naive_date || has_aware_date; }

  [[nodiscard]] Dtype dtype(std::size_t rows) const {
    if (present == 0) return Dtype::kEmpty;
    const bool missing = present < rows;
    if (has_date())
      return has_string || has_bool || has_int || has_float || (has_naive_date && has_aware_date)
                 ? Dtype::kObject
                 : Dtype::kDatetime;
    if (has_string) return Dtype::kObject;
    if (has_bool)
      return has_int || has_float || missing ? Dtype::kObject : Dtype::kBool;
    if (has_float || missing) return Dtype::kFloat64;
    return Dtype::kInt64;
  }
};

// The dtype of two frames' column concatenated (pandas concat): equal dtypes
// stay, an empty side (the column missing there, so NaN cells) keeps the
// other's dtype save that ints and booleans then hold NaN, anything else is
// object; ints and floats make float64.
inline Dtype concat_dtype(Dtype left, Dtype right) {
  if (left == right) return left;
  if (left == Dtype::kEmpty) std::swap(left, right);
  if (right == Dtype::kEmpty)
    return left == Dtype::kInt64 ? Dtype::kFloat64 : left == Dtype::kBool ? Dtype::kObject : left;
  if ((left == Dtype::kInt64 && right == Dtype::kFloat64) || (left == Dtype::kFloat64 && right == Dtype::kInt64))
    return Dtype::kFloat64;
  return Dtype::kObject;
}

// Whether a catalog declares `key` of `owner` a date ("date", "time",
// "timestamp", ... in any case), as the reference OCEL 2.0 readers convert.
inline bool declared_date(const OcelTypeCatalog& catalog, const std::string& owner,
                          const std::string& key);
// A date attribute's string value as a timestamp text, or nullptr when the
// value is no string or the text is no timestamp (kept as written).
inline const std::string* date_value_text(const OcelValue& value);
// Whether `value` of `owner`'s `key` is a datetime in the reference frame: a
// text the catalog declares a date and that reads as a timestamp (the native
// readers keep the texts the reference keeps as texts unread).
inline bool reference_date_value(const OcelTypeCatalog& catalog, const std::string& owner,
                                 const std::string& key, const OcelValue& value) {
  return date_value_text(value) != nullptr && declared_date(catalog, owner, key);
}

// Columns in first-appearance order with their value statistics.
class Columns {
 public:
  void observe(const std::string& key, const OcelValue& value) {
    auto [slot, added] = index_.try_emplace(key, names_.size());
    if (added) {
      names_.push_back(key);
      stats_.emplace_back();
    }
    stats_[slot->second].observe(value);
  }
  void observe(const std::string& key, const OcelValue& value, bool date) {
    auto [slot, added] = index_.try_emplace(key, names_.size());
    if (added) {
      names_.push_back(key);
      stats_.emplace_back();
    }
    stats_[slot->second].observe(value, date);
  }
  void observe_row(const std::vector<OcelAttribute>& attributes) {
    ++rows_;
    for (const auto& attribute : attributes) observe(attribute.key, attribute.value);
  }
  // Round EK: a row of `owner`, its date values (by `catalog`) as datetimes.
  void observe_row(const std::vector<OcelAttribute>& attributes, const OcelTypeCatalog& catalog,
                   const std::string& owner) {
    ++rows_;
    for (const auto& attribute : attributes)
      observe(attribute.key, attribute.value,
              reference_date_value(catalog, owner, attribute.key, attribute.value));
  }
  void add_rows(std::size_t rows) { rows_ += rows; }

  [[nodiscard]] const std::vector<std::string>& names() const { return names_; }
  // The column's position in first-appearance order (the end for no column).
  [[nodiscard]] std::size_t position(const std::string& key) const {
    const auto found = index_.find(key);
    return found == index_.end() ? names_.size() : found->second;
  }
  [[nodiscard]] std::size_t rows() const { return rows_; }
  [[nodiscard]] const ColumnStats* stats(const std::string& key) const {
    const auto found = index_.find(key);
    return found == index_.end() ? nullptr : &stats_[found->second];
  }
  [[nodiscard]] Dtype dtype(const std::string& key) const {
    const auto* found = stats(key);
    return found == nullptr ? Dtype::kEmpty : found->dtype(rows_);
  }

 private:
  std::vector<std::string> names_;
  std::vector<ColumnStats> stats_;
  std::unordered_map<std::string, std::size_t> index_;
  std::size_t rows_ = 0;
};

// Round EK: a row's attributes as the reference writers print a frame row:
// in the frame's column order (first appearance over the rows), null cells
// left out unless `keep_null`; a repeated key keeps its last value.
inline std::vector<const OcelAttribute*> frame_row(const std::vector<OcelAttribute>& attributes,
                                                   const Columns& frame, bool keep_null = false) {
  std::vector<const OcelAttribute*> out;
  std::unordered_map<std::string_view, std::size_t> seen;
  for (const auto& attribute : attributes) {
    const auto [slot, added] = seen.try_emplace(attribute.key, out.size());
    if (added) out.push_back(&attribute);
    else out[slot->second] = &attribute;
  }
  if (!keep_null)
    std::erase_if(out, [](const OcelAttribute* attribute) {
      return std::holds_alternative<std::nullptr_t>(attribute->value);
    });
  std::stable_sort(out.begin(), out.end(), [&](const OcelAttribute* left, const OcelAttribute* right) {
    return frame.position(left->key) < frame.position(right->key);
  });
  return out;
}

// One owner's attribute declarations in output order, per owner.
using TypeDeclarations =
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>;

// The reference OCEL 2.0 writers (write_ocel2_xml, write_ocel2_json) and
// write_ocel_json's type catalogs declare what each type's rows hold
// (observed black-box, round EK): "float" for a float64 frame column (an
// integer column with a missing cell is one), "date" for a datetime64 one;
// in an object column, by the type's own values: "date" when all are
// datetimes, "float" when floats with no text or datetime (ints and booleans
// next to them), otherwise "string" (also integer and boolean columns). Each
// type lists its attributes with a value in global column order (changed
// fields after the static ones, typed by the changes frame when the type
// holds no static value); declared-only catalog entries follow with their
// declared type, so empty types and attributes survive.
inline std::string reference_declared_type(Dtype column, const ColumnStats* owner) {
  if (column == Dtype::kFloat64) return "float";
  if (column == Dtype::kDatetime) return "date";
  if (column != Dtype::kObject || owner == nullptr) return "string";
  if (owner->has_string) return "string";
  if (owner->has_date())
    return owner->has_bool || owner->has_int || owner->has_float || (owner->has_naive_date && owner->has_aware_date)
               ? "string"
               : "date";
  return owner->has_float ? "float" : "string";
}

inline void reference_type_declarations(const Ocel& log, TypeDeclarations& object_types,
                                        TypeDeclarations& event_types) {
  Columns object_columns, change_columns, event_columns;
  // Per owner: the stats of its own rows (objects frame and changes frame).
  std::map<std::string, Columns> object_owner, change_owner, event_owner;
  std::unordered_map<std::string_view, std::string_view> object_type_of;
  for (const auto& object : log.objects) {
    object_type_of.emplace(object.id, object.type);
    object_columns.observe_row(object.attributes, log.object_type_attributes, object.type);
    object_owner[object.type].observe_row(object.attributes, log.object_type_attributes, object.type);
  }
  for (const auto& change : log.object_changes) {
    std::string owner = change.object_type;
    if (owner.empty())
      if (const auto found = object_type_of.find(change.object_id); found != object_type_of.end())
        owner = std::string(found->second);
    const bool date = reference_date_value(log.object_type_attributes, owner, change.field, change.value);
    change_columns.add_rows(1);
    change_columns.observe(change.field, change.value, date);
    auto& own = change_owner[owner];
    own.add_rows(1);
    own.observe(change.field, change.value, date);
  }
  for (const auto& event : log.events) {
    event_columns.observe_row(event.attributes, log.event_type_attributes, event.activity);
    event_owner[event.activity].observe_row(event.attributes, log.event_type_attributes, event.activity);
  }
  const auto owner_stats = [](const std::map<std::string, Columns>& owners, const std::string& owner,
                              const std::string& key) -> const ColumnStats* {
    const auto found = owners.find(owner);
    if (found == owners.end()) return nullptr;
    const auto* stats = found->second.stats(key);
    return stats == nullptr || stats->present == 0 ? nullptr : stats;
  };
  using Present = std::map<std::string, std::unordered_set<std::string>>;
  const auto build = [&](const OcelTypeCatalog& catalog, const Present& present,
                         const std::vector<const Columns*>& order,
                         const std::vector<const std::map<std::string, Columns>*>& owners,
                         TypeDeclarations& table) {
    for (const auto& [owner, keys] : present) {
      auto& declarations = table[owner];
      std::unordered_set<std::string> written;
      for (const auto* columns : order)
        for (const auto& name : columns->names()) {
          if (!keys.contains(name) || !written.insert(name).second) continue;
          // The static frame's column types a type holding a static value,
          // the changes frame's one holding only changes.
          std::size_t frame = 0;
          const ColumnStats* own = owner_stats(*owners[0], owner, name);
          if (own == nullptr && order.size() > 1) {
            frame = 1;
            own = owner_stats(*owners[1], owner, name);
          }
          auto dtype = order[frame]->dtype(name);
          if (frame == 0 && dtype == Dtype::kEmpty && order.size() > 1) dtype = order[1]->dtype(name);
          declarations.emplace_back(name, reference_declared_type(dtype, own));
        }
    }
    for (const auto& [owner, schema] : catalog) {
      auto& declarations = table[owner];
      for (const auto& [name, type] : schema) {
        const bool listed = std::any_of(declarations.begin(), declarations.end(),
                                        [&](const auto& entry) { return entry.first == name; });
        if (!listed) declarations.emplace_back(name, type.empty() ? "string" : type);
      }
    }
  };
  Present object_present, event_present;
  for (const auto& object : log.objects)
    for (const auto& attribute : object.attributes)
      if (!std::holds_alternative<std::nullptr_t>(attribute.value))
        object_present[object.type].insert(attribute.key);
  for (const auto& change : log.object_changes)
    if (!std::holds_alternative<std::nullptr_t>(change.value))
      object_present[change.object_type].insert(change.field);
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes)
      if (!std::holds_alternative<std::nullptr_t>(attribute.value))
        event_present[event.activity].insert(attribute.key);
  build(log.object_type_attributes, object_present, {&object_columns, &change_columns},
        {&object_owner, &change_owner}, object_types);
  build(log.event_type_attributes, event_present, {&event_columns}, {&event_owner}, event_types);
  for (auto* table : {&object_types, &event_types})
    for (auto& [owner, declarations] : *table)
      std::erase_if(declarations, [](const auto& entry) { return entry.first.starts_with("ocel:"); });
  for (const auto& object : log.objects) object_types[object.type];
  for (const auto& event : log.events) event_types[event.activity];
}

// pandas to_sql's column type: from the dtype, or for an object column from
// the kinds of its non-null values in the written table. Round EK: ints with
// a missing cell in the table (`rows` its rows) are REAL there (observed
// black-box: the table's column is float64, an int stored as a float).
inline std::string sql_type(Dtype dtype, const ColumnStats& table_values, std::size_t rows = 0) {
  switch (dtype) {
    case Dtype::kInt64:
    case Dtype::kBool:
      return "INTEGER";
    case Dtype::kFloat64:
      return "REAL";
    case Dtype::kDatetime:
      return "TIMESTAMP";
    default:
      break;
  }
  if (table_values.has_string || table_values.has_date()) return "TEXT";
  if (table_values.has_bool)
    return table_values.has_int || table_values.has_float ? "TEXT" : "INTEGER";
  if (table_values.has_float) return "REAL";
  if (table_values.has_int) return table_values.present < rows ? "REAL" : "INTEGER";
  return "TEXT";
}

// Python's repr of a float; "nan", "inf" and "-inf" when not finite.
inline std::string float_repr(double value) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
  return filtering_float_repr(value);
}

// Python's str of a scalar: True/False, the int, the float repr, the text.
inline std::string python_str(const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return "None";
  if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "True" : "False";
  if (const auto* integer = std::get_if<std::int64_t>(&value)) return std::to_string(*integer);
  if (const auto* real = std::get_if<double>(&value)) return float_repr(*real);
  return std::get<std::string>(value);
}

// Python's repr of a str: single quotes unless the text holds a single quote
// and no double quote; backslash, the quote and control characters escaped.
// Bytes from 0x80 up are kept (non-ASCII text is printable).
inline std::string python_str_repr(std::string_view text) {
  const bool double_quote = text.find('\'') != std::string_view::npos &&
                            text.find('"') == std::string_view::npos;
  const char quote = double_quote ? '"' : '\'';
  std::string out(1, quote);
  for (const char ch : text) {
    const auto byte = static_cast<unsigned char>(ch);
    if (ch == '\\') out += "\\\\";
    else if (ch == quote) { out += '\\'; out += ch; }
    else if (ch == '\n') out += "\\n";
    else if (ch == '\r') out += "\\r";
    else if (ch == '\t') out += "\\t";
    else if (byte < 0x20 || byte == 0x7f) {
      static constexpr char digits[] = "0123456789abcdef";
      out += "\\x";
      out += digits[byte >> 4];
      out += digits[byte & 15];
    } else {
      out += ch;
    }
  }
  out += quote;
  return out;
}

// A parsed timestamp's civil date and time of day (UTC for an aware one,
// the wall clock for a naive one); nullopt outside years 1..9999.
struct CivilTimestamp {
  long long year, month, day, hour, minute, second;
  std::uint32_t nanoseconds;
  bool timezone_aware;
};

inline std::optional<CivilTimestamp> civil_timestamp(const ParsedExactTimestamp& parsed) {
  std::int64_t days = parsed.seconds / 86400;
  std::int64_t rest = parsed.seconds % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  // Civil date from days since 1970-01-01 (Howard Hinnant's algorithm).
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::int64_t doe = z - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  std::int64_t year = yoe + era * 400;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
  if (month <= 2) ++year;
  if (year < 1 || year > 9999) return std::nullopt;
  return CivilTimestamp{static_cast<long long>(year), static_cast<long long>(month),
                        static_cast<long long>(day), static_cast<long long>(rest / 3600),
                        static_cast<long long>(rest / 60 % 60), static_cast<long long>(rest % 60),
                        parsed.nanoseconds, parsed.timezone_aware};
}

// "Y-MM-DD<separator>HH:MM:SS" plus `digits` fraction digits (0, 3, 6 or 9),
// the year zero-padded to four digits when `pad_year`.
inline std::string civil_timestamp_text(const CivilTimestamp& time, char separator, int digits,
                                        bool pad_year) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, pad_year ? "%04lld-%02lld-%02lld%c%02lld:%02lld:%02lld"
                                                : "%lld-%02lld-%02lld%c%02lld:%02lld:%02lld",
                time.year, time.month, time.day, separator, time.hour, time.minute, time.second);
  std::string out(buffer);
  if (digits > 0) {
    unsigned fraction = time.nanoseconds;
    for (int drop = 9 - digits; drop > 0; --drop) fraction /= 10;
    std::snprintf(buffer, sizeof buffer, ".%0*u", digits, fraction);
    out += buffer;
  }
  return out;
}

// An event time pandas holds as NaT: the empty text native keeps for an
// OCEL 2.0 SQLite '' or NULL time, sent on in transports (round EG). pandas
// sorts it after every time and prints it "NaT".
inline bool nat_time_text(std::string_view text) { return text.empty(); }
// Round EK: a change time pandas reads as NaT from a text (its NaT strings:
// "NaT", "nat", "NAT", "nan", "NaN", "NAN"; observed black-box in
// read_ocel2_json's change frame), or the empty text.
inline bool pandas_nat_time_text(std::string_view text) {
  return text.empty() || text == "NaT" || text == "nat" || text == "NAT" || text == "nan" ||
         text == "NaN" || text == "NAN";
}
// The error pm4py raises taking .timestamp() of a missing event time: NaT's,
// or None's for a log whose time column holds None (Ocel::none_event_times,
// round EH).
inline std::string nat_timestamp_error(const Ocel& log) {
  return log.none_event_times ? "AttributeError: 'NoneType' object has no attribute 'timestamp'"
                              : "ValueError: NaTType does not support timestamp";
}

// The text pandas prints for a timestamp the reference holds as UTC:
// "YYYY-MM-DD HH:MM:SS[.ffffff]+00:00" (separator ' ', suffix "+00:00"), or
// with separator 'T' and suffix "Z" for the OCEL 1.0 XML date. A naive text
// gets no suffix; text that is no timestamp is returned as is.
// With `microseconds`, nanosecond digits are cut first, as a value passed
// through a Python datetime is (pm4py.write_ocel2_sqlite, round BW).
inline std::string utc_timestamp_text(std::string_view text, char separator,
                                      std::string_view suffix, bool microseconds = false) {
  auto parsed = parse_timestamp_exact(text);
  if (parsed && microseconds) parsed->nanoseconds -= parsed->nanoseconds % 1000;
  const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
  if (!time) return std::string(text);
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  auto out = civil_timestamp_text(*time, separator, digits, true);
  if (time->timezone_aware) out += suffix;
  return out;
}

// Round EK: a datetime cell as the reference JSON and OCEL 1.0 XML writers
// print a datetime64 column ("%Y-%m-%dT%H:%M:%SZ", a naive one too), the
// fraction kept (declared: pm4py drops it); text that is no timestamp stays.
inline std::string datetime64_json_text(std::string_view text) {
  const auto parsed = parse_timestamp_exact(text);
  const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
  if (!time) return std::string(text);
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  return civil_timestamp_text(*time, 'T', digits, true) + "Z";
}
// Round EK: a datetime cell of an object column (dates next to other values)
// as every reference writer prints it: its isoformat ("+00:00", a naive one
// without offset).
inline std::string object_datetime_text(std::string_view text) {
  return utc_timestamp_text(text, 'T', "+00:00");
}

// The texts DataFrame.to_csv writes for a timestamp column holding `texts`.
// A column of naive timestamps is datetime64 and formatted as a whole: dates
// alone ("2024-01-01", the year unpadded) when every value is a midnight,
// otherwise "Y-MM-DD HH:MM:SS" with as many fraction digits as the most
// precise value needs (3, 6 or 9) on every value. A column of aware
// timestamps is written value by value as str() of each: in the offset the
// values share (pm4py.read_ocel_csv keeps a fixed "+02:00"; the other native
// importers store UTC), and in UTC when they do not share one. A column
// mixing kinds or holding text that is no timestamp is written value by value
// too, as utc_timestamp_text with suffix "+00:00" (observed black-box on
// pm4py.write_ocel_csv).
inline std::vector<std::string> pandas_timestamp_column_text(
    const std::vector<std::string_view>& texts) {
  // Round EH: a NaT time (an empty text) is an empty cell, and the column's
  // format follows its other values, as pandas' to_csv writes it (observed
  // black-box).
  if (std::any_of(texts.begin(), texts.end(), [](std::string_view text) { return nat_time_text(text); })) {
    std::vector<std::string_view> known;
    for (const auto text : texts)
      if (!nat_time_text(text)) known.push_back(text);
    auto printed = pandas_timestamp_column_text(known);
    std::vector<std::string> out;
    out.reserve(texts.size());
    std::size_t next = 0;
    for (const auto text : texts) out.push_back(nat_time_text(text) ? std::string() : std::move(printed[next++]));
    return out;
  }
  std::vector<CivilTimestamp> times;
  times.reserve(texts.size());
  bool naive_column = true, aware_column = true;
  std::optional<std::int32_t> offset;  // minutes, when every aware value shares it
  bool one_offset = true;
  std::vector<ParsedExactTimestamp> instants;
  instants.reserve(texts.size());
  for (const auto text : texts) {
    const auto parsed = parse_timestamp_exact(text);
    const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
    if (!time) {
      naive_column = aware_column = false;
      break;
    }
    (time->timezone_aware ? naive_column : aware_column) = false;
    if (time->timezone_aware) {
      if (offset && *offset != parsed->offset_minutes) one_offset = false;
      offset = parsed->offset_minutes;
    }
    times.push_back(*time);
    instants.push_back(*parsed);
  }
  std::vector<std::string> out;
  out.reserve(texts.size());
  if (aware_column && one_offset && offset && *offset != 0) {
    char suffix[16];
    const auto minutes = *offset < 0 ? -*offset : *offset;
    std::snprintf(suffix, sizeof suffix, "%c%02d:%02d", *offset < 0 ? '-' : '+',
                  static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
    for (std::size_t index = 0; index < texts.size(); ++index) {
      auto local = instants[index];
      local.seconds += static_cast<std::int64_t>(*offset) * 60;
      const auto time = civil_timestamp(local);
      if (!time) {
        out.push_back(utc_timestamp_text(texts[index], ' ', "+00:00"));
        continue;
      }
      const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
      out.push_back(civil_timestamp_text(*time, ' ', digits, true) + suffix);
    }
    return out;
  }
  if (!naive_column || texts.empty()) {
    for (const auto text : texts) out.push_back(utc_timestamp_text(text, ' ', "+00:00"));
    return out;
  }
  bool dates_only = true;
  int digits = 0;
  for (const auto& time : times) {
    if (time.hour != 0 || time.minute != 0 || time.second != 0 || time.nanoseconds != 0)
      dates_only = false;
    if (time.nanoseconds % 1000 != 0) digits = 9;
    else if (time.nanoseconds % 1000000 != 0) digits = std::max(digits, 6);
    else if (time.nanoseconds != 0) digits = std::max(digits, 3);
  }
  for (const auto& time : times) {
    if (dates_only) {
      char buffer[32];
      std::snprintf(buffer, sizeof buffer, "%lld-%02lld-%02lld", time.year, time.month, time.day);
      out.emplace_back(buffer);
    } else {
      out.push_back(civil_timestamp_text(time, ' ', digits, false));
    }
  }
  return out;
}

// The offset (minutes) that every value of an aware timestamp column shares,
// when it is not zero. pm4py.read_ocel_csv keeps such a column with its
// fixed offset (datetime64[us, UTC+02:00]; mixed offsets raise there), and
// the reference writers print each value's wall clock in it (observed
// black-box: OCEL 2.0 XML "2024-01-01T10:00:00+02:00", OCEL 1.0 XML
// "2024-01-01T10:00:00Z", SQLite "2024-01-01 10:00:00+02:00"). The other
// native importers store UTC, which gives nullopt.
inline std::optional<std::int32_t> shared_nonzero_offset(
    const std::vector<std::string_view>& texts) {
  std::optional<std::int32_t> offset;
  for (const auto text : texts) {
    // Round EH: a NaT time leaves the column's offset as it is.
    if (nat_time_text(text)) continue;
    const auto parsed = parse_timestamp_exact(text);
    if (!parsed || !parsed->timezone_aware) return std::nullopt;
    if (offset && *offset != parsed->offset_minutes) return std::nullopt;
    offset = parsed->offset_minutes;
  }
  if (!offset || *offset == 0) return std::nullopt;
  return offset;
}

// `text` as utc_timestamp_text prints it, but in the wall clock of
// `offset_minutes` (nullopt: UTC) and followed by `suffix`, or by the offset
// itself ("+02:00") when `suffix` is nullopt.
inline std::string offset_timestamp_text(std::string_view text, char separator,
                                         std::optional<std::int32_t> offset_minutes,
                                         std::optional<std::string_view> suffix,
                                         bool microseconds = false) {
  if (!offset_minutes) return utc_timestamp_text(text, separator, suffix.value_or("+00:00"), microseconds);
  auto parsed = parse_timestamp_exact(text);
  if (!parsed || !parsed->timezone_aware)
    return utc_timestamp_text(text, separator, suffix.value_or("+00:00"), microseconds);
  if (microseconds) parsed->nanoseconds -= parsed->nanoseconds % 1000;
  parsed->seconds += static_cast<std::int64_t>(*offset_minutes) * 60;
  const auto time = civil_timestamp(*parsed);
  if (!time) return utc_timestamp_text(text, separator, suffix.value_or("+00:00"), microseconds);
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  auto out = civil_timestamp_text(*time, separator, digits, true);
  if (suffix) return out.append(*suffix);
  char buffer[16];
  const auto minutes = *offset_minutes < 0 ? -*offset_minutes : *offset_minutes;
  std::snprintf(buffer, sizeof buffer, "%c%02d:%02d", *offset_minutes < 0 ? '-' : '+',
                static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
  return out.append(buffer);
}

// Whether a catalog declares `key` of `owner` a date ("date", "time",
// "timestamp", ... in any case), as the reference OCEL 2.0 readers convert.
inline bool declared_date(const OcelTypeCatalog& catalog, const std::string& owner,
                          const std::string& key) {
  const auto schema = catalog.find(owner);
  if (schema == catalog.end()) return false;
  const auto found = schema->second.find(key);
  if (found == schema->second.end()) return false;
  std::string lowered = found->second;
  for (auto& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return lowered.find("date") != std::string::npos || lowered.find("time") != std::string::npos;
}

// A date attribute's string value as a timestamp text, or nullptr when the
// value is no string or the text is no timestamp (kept as written).
inline const std::string* date_value_text(const OcelValue& value) {
  const auto* text = std::get_if<std::string>(&value);
  if (text == nullptr || !parse_timestamp_exact(*text)) return nullptr;
  return text;
}

}  // namespace minera::ocel_frame
