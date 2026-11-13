#include "minera/pandas_attribute_repetition_filter.hpp"
#include "pandas_datetime_target.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

namespace minera {
namespace {

[[noreturn]] void error(std::string type, std::string message) {
  throw PandasAttributeRepetitionError(std::move(type), std::move(message));
}

std::string string_key_repr(std::string_view value) {
  const char quote = value.find('\'') != std::string_view::npos &&
                             value.find('"') == std::string_view::npos
                         ? '"' : '\'';
  std::string result(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t position = 0; position < value.size();) {
    const auto start = position;
    const auto first = static_cast<unsigned char>(value[position++]);
    char32_t code = first;
    unsigned continuation = 0;
    if (first < 0x80) {}
    else if (first >= 0xc2 && first <= 0xdf) { code = first & 31; continuation = 1; }
    else if (first >= 0xe0 && first <= 0xef) { code = first & 15; continuation = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; continuation = 3; }
    else error("ValueError", "attribute repetition selector has invalid UTF-8");
    for (unsigned i = 0; i < continuation; ++i) {
      if (position == value.size()) error("ValueError", "attribute repetition selector has short UTF-8");
      const auto next = static_cast<unsigned char>(value[position++]);
      if ((next & 0xc0) != 0x80) error("ValueError", "attribute repetition selector has invalid UTF-8 continuation");
      code = (code << 6) | (next & 63);
    }
    if ((continuation == 1 && code < 0x80) ||
        (continuation == 2 && code < 0x800) ||
        (continuation == 3 && code < 0x10000) || code > 0x10ffff)
      error("ValueError", "attribute repetition selector has invalid code point");
    if (code == static_cast<unsigned char>(quote) || code == '\\') {
      result += '\\'; result += static_cast<char>(code);
    } else if (code == '\n') result += "\\n";
    else if (code == '\r') result += "\\r";
    else if (code == '\t') result += "\\t";
    else if (!root_cause_unicode_printable(code)) {
      const int digits = code <= 0xff ? 2 : code <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        result += hex[(code >> shift) & 15];
    } else result.append(value.substr(start, position - start));
  }
  return result + quote;
}

std::string key_text(const PandasAttributeRepetitionScalar& key) {
  return std::visit([](const auto& value) -> std::string {
    using T=std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T,std::nullptr_t>) return "None";
    else if constexpr (std::is_same_v<T,bool>) return value ? "True" : "False";
    else if constexpr (std::is_same_v<T,std::string>) return string_key_repr(value);
    else if constexpr (std::is_same_v<T,TokenInteger>) return value.decimal();
    else return std::to_string(value);
  },key);
}

std::size_t column(const EventTable& source,
                     const PandasAttributeRepetitionScalar& key, bool grouping) {
  if (const auto* name=std::get_if<std::string>(&key)) {
    const auto found=std::find(source.columns.begin(),source.columns.end(),*name);
    if (found!=source.columns.end()) return static_cast<std::size_t>(found-source.columns.begin());
  } else if (grouping && std::holds_alternative<std::nullptr_t>(key)) {
    error("TypeError","You have to supply one of 'by' and 'level'");
  }
  error("KeyError",key_text(key));
}

bool date(const EventAttribute& value) {
  return value.kind==AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value);
}

std::optional<ParsedExactTimestamp> instant(const EventAttribute& value) {
  const auto* stored=std::get_if<NativeDate>(&value.typed_value);
  return parse_timestamp_exact(stored ? stored->value : value.value);
}

bool missing(const EventAttribute& value) {
  if (value.is_null || is_native_na_attribute(value)) return true;
  if (const auto* real=std::get_if<double>(&value.typed_value)) return std::isnan(*real);
  if (value.kind==AttributeKind::kFloat && std::holds_alternative<std::monostate>(value.typed_value))
    return std::isnan(native_attribute_number(value));
  return date(value) && !instant(value);
}

bool same_instant(const ParsedExactTimestamp& a, const ParsedExactTimestamp& b) {
  return a.timezone_aware==b.timezone_aware && a.seconds==b.seconds && a.nanoseconds==b.nanoseconds;
}

// A datetime column parses a string target and localizes a naive result to
// the column timezone; wall times without a single instant match nothing.
std::optional<ParsedExactTimestamp> column_instant(const EventAttribute& target,
                                                   const std::optional<std::string>& zone) {
  if (date(target)) return instant(target);
  // "NaT" and the other NaT texts match no cell (round BY).
  return pandas_datetime_target::instant(target.value,zone,
      "pandas datetime column string targets must be ISO-8601 timestamps");
}

// NumPy comparison family of a typed numeric column (or categorical categories).
enum class NumericFamily { kNone, kInteger, kFloat32, kFloat64, kBoolean, kCategoricalNumber, kCategoricalBoolean, kDouble };

NumericFamily numeric_family(std::string_view dtype) {
  std::string lower(dtype);
  for (auto& c:lower) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower=="bool" || lower=="boolean") return NumericFamily::kBoolean;
  if (lower=="float32") return NumericFamily::kFloat32;
  if (lower=="float64") return NumericFamily::kFloat64;
  if (lower.starts_with("int") || lower.starts_with("uint")) return NumericFamily::kInteger;
  return NumericFamily::kNone;
}

// A scalar number as an exact integer (booleans are 0/1) or a double.
std::optional<std::variant<TokenInteger,double>> number(const EventAttribute& value) {
  if (value.container!=AttributeContainerKind::kScalar || value.is_null || is_native_na_attribute(value)) return std::nullopt;
  if (const auto* flag=std::get_if<bool>(&value.typed_value)) return TokenInteger(*flag ? 1 : 0);
  if (value.kind==AttributeKind::kBoolean) return TokenInteger(value.value=="true" || value.value=="True" ? 1 : 0);
  if (const auto* whole=std::get_if<std::int64_t>(&value.typed_value)) return TokenInteger(*whole);
  if (const auto* whole=std::get_if<std::uint64_t>(&value.typed_value)) return TokenInteger(*whole);
  if (const auto* real=std::get_if<double>(&value.typed_value)) return *real;
  if (value.kind==AttributeKind::kInt) return TokenInteger(value.value);
  if (value.kind==AttributeKind::kFloat) return native_attribute_number(value);
  return std::nullopt;
}

bool boolean_scalar(const EventAttribute& value) {
  return value.container==AttributeContainerKind::kScalar &&
         (value.kind==AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value));
}

double as_double(const std::variant<TokenInteger,double>& value) {
  if (const auto* real=std::get_if<double>(&value)) return *real;
  return std::strtod(std::get<TokenInteger>(value).decimal().c_str(),nullptr);
}

float as_float32(const std::variant<TokenInteger,double>& value) {
  if (const auto* whole=std::get_if<TokenInteger>(&value)) return std::strtof(whole->decimal().c_str(),nullptr);
  const double real=std::get<double>(value);
  // Round-to-nearest overflow threshold of binary32.
  if (std::fabs(real)>=0x1.ffffffp127) return std::copysign(HUGE_VALF,real);
  return static_cast<float>(real);
}

// NumPy 2 compares a typed column with a weak Python scalar: integers exactly,
// integer columns with floats in float64, float32 columns in float32.
std::optional<bool> numeric_equal(const EventAttribute& cell, const EventAttribute& target, NumericFamily family) {
  if (family==NumericFamily::kNone) return std::nullopt;
  if (family==NumericFamily::kCategoricalBoolean) return boolean_scalar(target) && root_cause_scalar_keys_equal(cell,target);
  if (family==NumericFamily::kCategoricalNumber && boolean_scalar(target)) return false;
  const auto a=number(cell),b=number(target);
  if (!a || !b) return std::nullopt;
  if (family==NumericFamily::kCategoricalNumber) return std::nullopt;
  if (family==NumericFamily::kFloat32) return as_float32(*a)==as_float32(*b);
  if (family==NumericFamily::kDouble) return as_double(*a)==as_double(*b);
  if (std::holds_alternative<TokenInteger>(*a) && std::holds_alternative<TokenInteger>(*b))
    return std::get<TokenInteger>(*a)==std::get<TokenInteger>(*b);
  return as_double(*a)==as_double(*b);
}

// A row-aligned list becomes a NumPy array first: any float makes it float64
// (every element compared in float64), integers stay exact unless the column
// is floating, and lists with other values become object arrays compared per
// element, where a float32 column element is already a Python float.
NumericFamily vector_family(const EventAttribute& target, NumericFamily column) {
  if (column==NumericFamily::kNone || column==NumericFamily::kCategoricalNumber ||
      column==NumericFamily::kCategoricalBoolean) return column;
  bool any_float=false, numeric=true;
  for (const auto& child:target.children) {
    const auto value=number(child);
    if (!value) { numeric=false; break; }
    if (std::holds_alternative<double>(*value)) any_float=true;
    else if (!boolean_scalar(child)) {
      const auto& whole=std::get<TokenInteger>(*value);
      if (whole<TokenInteger(std::numeric_limits<std::int64_t>::min()) ||
          whole>TokenInteger(std::numeric_limits<std::uint64_t>::max())) numeric=false;
    }
  }
  if (!numeric) return column==NumericFamily::kFloat32 ? NumericFamily::kFloat64 : column;
  if (any_float) return NumericFamily::kDouble;
  return column==NumericFamily::kFloat32 || column==NumericFamily::kFloat64 ? NumericFamily::kDouble : column;
}

bool scalar_equal(const EventAttribute& left, const EventAttribute& right,
                    bool datetime_column, const std::optional<std::string>& zone,
                    NumericFamily family=NumericFamily::kNone) {
  if (missing(left) || missing(right)) return false;
  if (const auto numeric=numeric_equal(left,right,family)) return *numeric;
  if (datetime_column) {
    const auto a=instant(left);
    if (!date(right) && (right.kind!=AttributeKind::kString ||
                        right.container!=AttributeContainerKind::kScalar)) return false;
    const auto b=column_instant(right,zone);
    return a && b && same_instant(*a,*b);
  }
  if (date(left) || date(right)) {
    if (!date(left) || !date(right)) return false;
    const auto a=instant(left),b=instant(right);
    return a && b && same_instant(*a,*b);
  }
  if (left.container!=right.container) return false;
  if (!root_cause_key_is_hashable(left) || !root_cause_key_is_hashable(right)) {
    // Native scalar source cells do not become equal to mutable containers.
    if (left.container==AttributeContainerKind::kScalar || right.container==AttributeContainerKind::kScalar)
      return false;
    error("TypeError","unsupported mutable dataframe comparison");
  }
  return root_cause_scalar_keys_equal(left,right);
}

std::string group_key(const EventAttribute& value) {
  if (!root_cause_key_is_hashable(value)) {
    const auto kind=value.container==AttributeContainerKind::kList ? "list"
        : value.container==AttributeContainerKind::kContainer ? "dict" : "set";
    error("TypeError",std::string("unhashable type: '")+kind+"'");
  }
  if (date(value)) {
    const auto parsed=instant(value);
    if (!parsed) return "missing";
    return std::string(parsed->timezone_aware ? "DA" : "DN")+
        std::to_string(parsed->seconds)+":"+std::to_string(parsed->nanoseconds);
  }
  return root_cause_scalar_key_fingerprint(value);
}

bool bound_accepts(std::uint64_t count, const PandasAttributeRepetitionScalar& bound,
                    bool lower) {
  return std::visit([&](const auto& value) -> bool {
    using T=std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T,std::nullptr_t>) return false;
    else if constexpr (std::is_same_v<T,std::string>) {
      error("TypeError","Invalid comparison between dtype=int64 and str");
    } else if constexpr (std::is_same_v<T,TokenInteger>) {
      return lower ? value<=TokenInteger(count) : TokenInteger(count)<=value;
    } else if constexpr (std::is_same_v<T,bool>) {
      return lower ? std::uint64_t(value)<=count : count<=std::uint64_t(value);
    } else {
      if (std::isnan(value)) return false;
      if (lower) return value<=0 || (value<0x1p64 && count>=static_cast<std::uint64_t>(std::ceil(value)));
      return value>=0 && (value>=0x1p64 || count<=static_cast<std::uint64_t>(std::floor(value)));
    }
  },bound);
}

bool arithmetic_index(const std::vector<EventTableIndexLabel>& labels) {
  std::vector<TokenInteger> numbers;
  numbers.reserve(labels.size());
  for (const auto& label:labels) {
    const auto* value=std::get_if<EventAttribute>(&label.value);
    if (!value || value->is_null) return false;
    if (const auto* number=std::get_if<std::int64_t>(&value->typed_value)) numbers.emplace_back(*number);
    else if (const auto* number=std::get_if<std::uint64_t>(&value->typed_value)) numbers.emplace_back(*number);
    else return false;
  }
  if (numbers.size()<2) return true;
  const TokenInteger step=numbers[1]-numbers[0];
  if (step==0) return false;
  for (std::size_t i=2;i<numbers.size();++i)
    if (numbers[i]-numbers[i-1]!=step) return false;
  return true;
}

}  // namespace

EventTable filter_pandas_attribute_value_repetition(
    const EventTable& source, const EventAttribute& target,
    const PandasAttributeRepetitionOptions& options) {
  source.validate();
  const auto selected=column(source,options.attribute_key,false);
  const bool vector_query=target.container==AttributeContainerKind::kList;
  if (vector_query && target.children.size()!=source.rows.size())
    error("ValueError","('Lengths must match to compare', ("+std::to_string(source.rows.size())+
        ",), ("+std::to_string(target.children.size())+",))");
  const std::string_view dtype=source.dtypes.empty() ? std::string_view("object") : std::string_view(source.dtypes[selected]);
  if (target.container!=AttributeContainerKind::kScalar && !vector_query && dtype!="object")
    // Typed columns treat tuples, sets and maps as dtype-specific array-likes.
    throw std::invalid_argument("pandas repetition targets for typed columns must be scalars or row-aligned lists");
  if ((dtype=="bool" || dtype=="boolean") && !vector_query && target.kind==AttributeKind::kInt &&
      !target.is_null && !std::holds_alternative<std::int64_t>(target.typed_value))
    error("OverflowError","Python int too large to convert to C long");
  const auto grouped=column(source,options.case_id_key,true);
  // A categorical column compares like its categories, except that numeric
  // categories never equal booleans and boolean categories only booleans.
  std::string_view compared=dtype;
  bool categorical=false;
  if (dtype=="category" && selected<source.categorical_dtypes.size() && source.categorical_dtypes[selected]) {
    compared=source.categorical_dtypes[selected]->categories_dtype;
    categorical=true;
  }
  const bool datetime_column=compared.starts_with("datetime64");
  std::optional<std::string> zone;
  if (datetime_column) if (const auto comma=compared.find(", "); comma!=std::string_view::npos)
    zone=std::string(compared.substr(comma+2,compared.size()-comma-3));
  auto family=numeric_family(compared);
  if (categorical && family!=NumericFamily::kNone)
    family=family==NumericFamily::kBoolean ? NumericFamily::kCategoricalBoolean : NumericFamily::kCategoricalNumber;
  if (vector_query) family=categorical ? NumericFamily::kNone : vector_family(target,family);
  std::unordered_map<std::string,std::uint64_t> counts;
  for (std::size_t i=0;i<source.rows.size();++i) {
    const auto& candidate=source.rows[i][selected];
    const auto& wanted=vector_query ? target.children[i] : target;
    if (!candidate || !scalar_equal(*candidate,wanted,datetime_column,zone,family)) continue;
    const auto& key=source.rows[i][grouped];
    if (!key || missing(*key)) continue;
    ++counts[group_key(*key)];
  }
  // Vectorized dataframe comparisons evaluate both bound operands.
  if (std::holds_alternative<std::string>(options.minimum) ||
      std::holds_alternative<std::string>(options.maximum))
    error("TypeError","Invalid comparison between dtype=int64 and str");
  std::unordered_map<std::string,bool> eligible;
  for (const auto& [key,count]:counts) {
    const bool lower=bound_accepts(count,options.minimum,true);
    const bool upper=bound_accepts(count,options.maximum,false);
    eligible.emplace(key,lower && upper);
  }
  EventTable result=source;
  result.rows.clear();
  if (result.index) result.index->labels.clear();
  std::vector<std::size_t> positions;
  for (std::size_t i=0;i<source.rows.size();++i) {
    const auto& key=source.rows[i][grouped];
    if (!key || missing(*key)) continue;
    // Unmatched mutable case values are irrelevant to grouping and isin.
    if (!root_cause_key_is_hashable(*key)) continue;
    const auto found=eligible.find(group_key(*key));
    if (found==eligible.end() || !found->second) continue;
    result.rows.push_back(source.rows[i]);
    positions.push_back(i);
    if (result.index) result.index->labels.push_back(source.index->labels[i]);
  }
  if (!result.index && positions.size()!=source.rows.size()) {
    EventTableIndex index;
    for (const auto position:positions) {
      EventAttribute value{"",std::to_string(position),AttributeKind::kInt};
      value.typed_value=static_cast<std::uint64_t>(position);
      index.labels.push_back({std::move(value)});
    }
    result.index=std::move(index);
  }
  if (result.index && result.index->kind=="RangeIndex" && !arithmetic_index(result.index->labels))
    result.index->kind="Index";
  return result;
}

}  // namespace minera
