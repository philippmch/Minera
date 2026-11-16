#include "minera/polars_variants_filter.hpp"
#include "polars_group_keys.hpp"
#include "polars_datetime.hpp"
#include "polars_float_text.hpp"
#include "polars_sort.hpp"
#include "polars_column_hint.hpp"
#include "polars_key_selection.hpp"
#include "polars_lit_model.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <charconv>
#include <bit>
#include <limits>
#include <cmath>
#include <numeric>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {

using Cell = std::optional<EventAttribute>;
using EnumRanks = std::unordered_map<std::string, std::size_t>;
enum class Dtype { kString, kEnum, kInteger, kFloat, kBoolean, kNull, kDate };

bool categorical_dtype(const std::string& value) {
  return value == "Categorical" || (value.starts_with("Categorical(") && value.ends_with(')'));
}
bool enum_dtype(const std::string& value) {
  return value.starts_with("Enum(") && value.ends_with(')');
}

[[noreturn]] void fail(std::string type, std::string phase, std::string message) {
  throw PolarsVariantsFilterError(std::move(type), std::move(phase), std::move(message));
}

bool null(const EventAttribute& value) { return value.is_null || is_native_na_attribute(value); }
bool collection(const EventAttribute& value) { return value.container != AttributeContainerKind::kScalar; }
bool boolean(const EventAttribute& value) {
  return value.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value);
}
bool integer(const EventAttribute& value) {
  return value.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) ||
         std::holds_alternative<std::uint64_t>(value.typed_value);
}
bool real(const EventAttribute& value) {
  return value.kind == AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value);
}
bool date(const EventAttribute& value) {
  return value.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value);
}
bool text(const EventAttribute& value) {
  return !collection(value) && !null(value) && !boolean(value) && !integer(value) && !real(value) && !date(value);
}
TokenInteger exact(const EventAttribute& value) {
  if (const auto* v = std::get_if<std::int64_t>(&value.typed_value)) return TokenInteger(*v);
  if (const auto* v = std::get_if<std::uint64_t>(&value.typed_value)) return TokenInteger(*v);
  if (const auto* v = std::get_if<bool>(&value.typed_value)) return TokenInteger(*v ? 1 : 0);
  if (boolean(value)) return TokenInteger(value.value == "true" || value.value == "1" ? 1 : 0);
  return TokenInteger(value.value);
}
double number(const EventAttribute& value) {
  return integer(value) || boolean(value) ? static_cast<double>(exact(value)) : native_attribute_number(value);
}
std::string real_text(double value, bool cast_string = false) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  char buffer[128];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (result.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars real value");
  std::string output(buffer, result.ptr);
  if (cast_string && output.find_first_of(".eE") == std::string::npos) output += ".0";
  return output;
}
std::string quote(const std::string& value) { return '"' + value + '"'; }
std::string debug_quote(const std::string& value) {
  std::string output = "\"";
  for (const auto c : value) {
    switch (c) {
      case '\\': output += "\\\\"; break;
      case '"': output += "\\\""; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      case '\0': output += "\\0"; break;
      default: output += c;
    }
  }
  return output + '"';
}
std::string python_type(const EventAttribute& value) {
  if (collection(value)) {
    switch (value.container) {
      case AttributeContainerKind::kList: return "list";
      case AttributeContainerKind::kTuple: return "tuple";
      case AttributeContainerKind::kSet: return "set";
      case AttributeContainerKind::kFrozenSet: return "frozenset";
      default: return "dict";
    }
  }
  if (null(value)) return "NoneType";
  if (boolean(value)) return "bool";
  if (integer(value)) return "int";
  if (real(value)) return "float";
  if (date(value)) return "datetime";
  return "str";
}
// The type name CPython's own TypeError and AttributeError messages print.
std::string core_type(const EventAttribute& value) {
  if (!collection(value) && !null(value) && date(value)) {
    const auto* native = std::get_if<NativeDate>(&value.typed_value);
    return (native ? native->value : value.value).size() == 10 ? "datetime.date" : "datetime.datetime";
  }
  return python_type(value);
}


Dtype dtype(const std::string& value) {
  if (value == "String" || categorical_dtype(value)) return Dtype::kString;
  if (enum_dtype(value)) return Dtype::kEnum;
  if (value == "Int64" || value == "UInt64" || value == "UInt32" || value == "UInt16" || value == "UInt8" || value == "Int32" || value == "Int16" || value == "Int8") return Dtype::kInteger;
  if (value == "Float64" || value == "Float32") return Dtype::kFloat;
  if (value == "Boolean") return Dtype::kBoolean;
  if (value == "Null") return Dtype::kNull;
  if (polars_datetime::temporal(value)) return Dtype::kDate;
  throw std::invalid_argument("Unsupported native Polars variants dtype: " + value);
}
std::string frame_plan(const EventTable& source) {
  std::string output = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, source.columns.size()); ++i) {
    if (i) output += ", ";
    output += quote(source.columns[i]);
  }
  if (source.columns.size() > 4) output += ", ...";
  return output + "]; PROJECT */" + std::to_string(source.columns.size()) + " COLUMNS";
}

int compare(const Cell& left, const Cell& right, Dtype kind,
            const EnumRanks* dictionary = nullptr) {
  const auto absent = [kind](const Cell& value) { return !value || null(*value) || kind == Dtype::kNull; };
  if (absent(left) || absent(right)) return absent(left) == absent(right) ? 0 : absent(left) ? -1 : 1;
  if (kind == Dtype::kString) return left->value < right->value ? -1 : right->value < left->value ? 1 : 0;
  if (kind == Dtype::kEnum) {
    if (!dictionary) throw std::invalid_argument("Native Polars variants Enum ordering requires its category dictionary");
    const auto position = [&](const std::string& value) {
      const auto found = dictionary->find(value);
      if (found == dictionary->end())
        throw std::invalid_argument("Native Polars variants Enum cell is absent from its category dictionary");
      return found->second;
    };
    const auto a = position(left->value), b = position(right->value);
    return a < b ? -1 : a > b ? 1 : 0;
  }
  if (kind == Dtype::kInteger || kind == Dtype::kBoolean) {
    const auto a = exact(*left), b = exact(*right); return a < b ? -1 : a > b ? 1 : 0;
  }
  if (kind == Dtype::kFloat) {
    const auto a = number(*left), b = number(*right);
    if (std::isnan(a) || std::isnan(b)) return std::isnan(a) == std::isnan(b) ? 0 : std::isnan(a) ? 1 : -1;
    return a < b ? -1 : a > b ? 1 : 0;
  }
  const auto instant = [](const EventAttribute& value) {
    const auto* stored = std::get_if<NativeDate>(&value.typed_value);
    const auto parsed = parse_timestamp_exact(stored ? stored->value : value.value);
    if (!parsed) throw std::invalid_argument("Unsupported native Polars variants temporal cell");
    return *parsed;
  };
  const auto a = instant(*left), b = instant(*right);
  if (a.seconds != b.seconds) return a.seconds < b.seconds ? -1 : 1;
  return a.nanoseconds < b.nanoseconds ? -1 : a.nanoseconds > b.nanoseconds ? 1 : 0;
}

std::string string_cast(const EventAttribute& value, const std::string& declared = "") {
  if (declared == "String" || categorical_dtype(declared) || enum_dtype(declared)) return value.value;
  if (text(value)) return value.value;
  if (boolean(value)) return static_cast<bool>(exact(value)) ? "true" : "false";
  if (integer(value)) return exact(value).decimal();
  // Round CH: Polars' Float to String cast (1e-5 as "0.00001", a Float32
  // column by its float digits).
  if (real(value)) return detail::polars_cast_float(number(value), declared == "Float32");
  if (date(value)) {
    const auto lexical=std::get_if<NativeDate>(&value.typed_value)?std::get<NativeDate>(value.typed_value).value:value.value;
    if(declared=="Date")return lexical.substr(0,10);
    // The column unit's fraction digits (round BX).
    const auto cast=polars_datetime::cast_text(lexical,declared);if(!cast)throw std::invalid_argument("Native Polars variants temporal cast requires valid declared datetime");
    return *cast;
  }
  if (value.container == AttributeContainerKind::kContainer) {
    std::string output = "{";
    for (std::size_t i = 0; i < value.children.size(); ++i) {
      if (i) output += ',';
      output += string_cast(value.children[i]);
    }
    return output + '}';
  }
  throw std::invalid_argument("Unsupported native Polars variants string cast");
}

bool truth(const EventAttribute& value) {
  if (null(value)) return false;
  if (collection(value)) return !value.children.empty();
  if (integer(value) || boolean(value)) return static_cast<bool>(exact(value));
  if (real(value)) return number(value) != 0;
  return !value.value.empty();
}
std::string group_key(const std::optional<EventAttribute>& cell, const std::string& dtype) {
  if (!cell || null(*cell) || dtype == "Null") return "n";
  const auto& v = *cell;
  if (dtype == "String" || dtype == "Categorical" || dtype.starts_with("Categorical(") || dtype.starts_with("Enum(")) return "s" + v.value;
  if (dtype == "Float64" || dtype == "Float32") {
    const auto n = number(v);
    return std::isnan(n) ? "fNaN" : n == 0 ? "f0" : "f" + std::to_string(std::bit_cast<std::uint64_t>(n));
  }
  if (dtype == "Boolean" || dtype == "Int64" || dtype == "UInt64" || dtype == "UInt32" || dtype == "Int32" || dtype == "UInt16" || dtype == "Int16" || dtype == "UInt8" || dtype == "Int8") return "i" + exact(v).decimal();
  if (polars_datetime::temporal(dtype)) {
    const auto parsed = polars_datetime::ticks(std::get_if<NativeDate>(&v.typed_value) ? std::get<NativeDate>(v.typed_value).value : v.value, polars_datetime::column_unit(dtype));
    if (!parsed) throw std::invalid_argument("Native Polars case grouping requires valid declared temporal cells");
    return "t" + std::to_string(*parsed);
  }
  throw std::invalid_argument("Unsupported native Polars case dtype: " + dtype);
}
std::vector<EventAttribute> iter(const EventAttribute& value) {
  if(value.container==AttributeContainerKind::kContainer){std::vector<EventAttribute> keys;for(const auto& child:value.children)keys.push_back({"",child.key});return keys;}
  if(collection(value))return value.children;
  if(text(value)){std::vector<EventAttribute> chars;for(std::size_t i=0;i<value.value.size();){const auto c=static_cast<unsigned char>(value.value[i]);const auto n=c<0x80?1:c<0xe0?2:c<0xf0?3:4;chars.push_back({"",value.value.substr(i,n)});i+=n;}return chars;}
  fail("TypeError","apply","'"+core_type(value)+"' object is not iterable");
}
std::string join_type(const EventAttribute& value) {
  if(date(value)){const auto lexical=std::get_if<NativeDate>(&value.typed_value)?std::get<NativeDate>(value.typed_value).value:value.value;return lexical.size()==10?"datetime.date":"datetime.datetime";}
  return core_type(value);
}
std::string join(const EventAttribute& sequence) {
  if(!collection(sequence)&&!text(sequence))fail("TypeError","apply","can only join an iterable");
  const auto items=iter(sequence);std::string result;
  for(std::size_t i=0;i<items.size();++i){if(!text(items[i]))fail("TypeError","apply","sequence item "+std::to_string(i)+": expected str instance, "+join_type(items[i])+" found");if(i)result+=',';result+=items[i].value;}
  return result;
}
std::vector<std::string> admitted(const EventAttribute& values){std::vector<std::string> result;for(const auto& sequence:iter(values))result.push_back(join(sequence));return result;}
std::string contexts(const std::vector<std::string>& nodes){std::string out="\n\nThis error occurred with the following context stack:\n";for(std::size_t i=0;i<nodes.size();++i)out+="\t["+std::to_string(i+1)+"] '"+nodes[i]+"'\n";return out;}
// Round CN: a dict timestamp key sorts the statistics as its struct literal.
std::string sort_time(const PolarsVariantsFilterOptions& options,bool statistics){
  if(statistics&&options.timestamp_key.container==AttributeContainerKind::kContainer)return detail::polars_lit::dict_literal_display(options.timestamp_key);
  // Round CS: a list timestamp key sorts as its Series literal (fresh CK
  // bank 2026111691 case 183).
  if(statistics&&(options.timestamp_key.container==AttributeContainerKind::kList||options.timestamp_key.container==AttributeContainerKind::kTuple))
    return detail::polars_lit::literal_list_display(detail::polars_lit::argument_literal(options.timestamp_key));
  return "col("+quote(statistics?options.timestamp_key.value:"time:timestamp")+")";
}
std::string sort_plan(const EventTable& source,const PolarsVariantsFilterOptions& options,bool statistics){return "SORT BY [col("+quote(options.case_id_key.value)+"), "+sort_time(options,statistics)+"]\n  "+frame_plan(source);}
namespace keys = detail::polars_keys;
std::string missing_prefix(const std::string& key,const std::vector<std::string>& columns){if(keys::by_name(key))return keys::by_name_message(key);std::string out="unable to find column "+debug_quote(key)+"; valid columns: [";for(std::size_t i=0;i<columns.size();++i){if(i)out+=", ";out+=debug_quote(columns[i]);}out+=']';if (const auto hint = detail::polars_column_hint(key, columns)) out += "\n\nDid you mean " + debug_quote(*hint) + '?';return out;}
[[noreturn]] void missing(const EventTable& source,const std::string& key,bool activity,bool statistics,const PolarsVariantsFilterOptions& options,const std::string& phase,bool eager_source){
  if(eager_source){
    const auto prefix=missing_prefix(key,source.columns);
    if(!activity)fail("ColumnNotFoundError",phase,prefix);
    fail("ColumnNotFoundError",phase,prefix+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n"+frame_plan(source)+contexts({"group_by"}));
  }
  std::vector<std::string> nodes;if(!activity)nodes.push_back("sort");nodes.push_back("group_by");
  if(!statistics){nodes.insert(nodes.end(),{"filter","select","join right","join"});}
  fail("ColumnNotFoundError",phase,missing_prefix(key,source.columns)+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '"+(statistics?"group_by":"join")+"' <---\n"+(activity?sort_plan(source,options,statistics):frame_plan(source))+contexts(nodes));
}
// Round CM: group_by(case) refuses a dict before anything else is built; a
// list activity key names the first item that is not a str (pl.col), and a
// dict or list of names resolves by name in grouped().
void validate(const PolarsVariantsFilterOptions& options,bool statistics=false){
  if(keys::dict_key(options.case_id_key))fail("TypeError","apply",keys::dict_argument_message());
  // Round CN: a list or tuple case key is a Series literal (built strictly)
  // before anything else is read.
  if(options.case_id_key.container==AttributeContainerKind::kList||options.case_id_key.container==AttributeContainerKind::kTuple)
    if(const auto error=detail::polars_lit::argument_literal_error(options.case_id_key))fail("TypeError","apply",*error);
  if(!text(options.activity_key)&&!collection(options.activity_key))fail("TypeError","apply","invalid input for `col`\n\nExpected `str` or `DataType`, got '"+python_type(options.activity_key)+"'.");
  const auto& act=options.activity_key;
  if(act.container==AttributeContainerKind::kList||act.container==AttributeContainerKind::kTuple){
    if(!act.children.empty()&&!text(act.children.front()))fail("TypeError","apply","invalid input for `col`\n\nExpected iterable of type `str` or `DataType`, got iterable of type '"+python_type(act.children.front())+"'.");
    for(const auto& item:act.children)if(!text(item))fail("TypeError","apply",null(item)?"'None' is not an instance of 'str'":"'"+python_type(item)+"' object is not an instance of 'str'");
  }
  if(null(options.case_id_key)&&!statistics)fail("ValueError","apply","must specify `on` OR `left_on` and `right_on`");
}
// Round CS: a list (or int) case key groups and joins by each item, a str
// item a column and any other value a literal named "literal", after one
// Series literal sorts the frame. The plan resolves the group_by keys before
// the activity column; no admitted variant folds the matching cases to the
// literal-only selection (one row holding the literal), and a one-row frame
// survives the sort (CK bank 2026111671 cases 369, 425).
bool listed_key(const EventAttribute& key){return key.container==AttributeContainerKind::kList||key.container==AttributeContainerKind::kTuple;}
std::vector<EventAttribute> key_items(const EventAttribute& key){if(listed_key(key))return key.children;return {key};}
std::optional<std::string> duplicate_key(const EventAttribute& key){
  std::vector<std::string> seen;
  for(const auto& item:key_items(key)){auto name=text(item)?item.value:std::string("literal");if(std::find(seen.begin(),seen.end(),name)!=seen.end())return name;seen.push_back(std::move(name));}
  return std::nullopt;
}
std::string literal_sort_plan(const EventTable& source,const PolarsVariantsFilterOptions& options,const std::string& time){
  namespace lit=detail::polars_lit;
  // Round CT: an int key sorts as its scalar literal (probe ct/l2).
  const auto& key=options.case_id_key;
  const auto shown=collection(key)?lit::literal_list_display(lit::argument_literal(key)):lit::scalar_display(lit::value_of(key));
  return "SORT BY ["+shown+", "+time+"]\n  "+frame_plan(source);
}
void duplicate_keys(const EventTable& source,const PolarsVariantsFilterOptions& options,bool statistics,const std::string& phase,const std::string& time){
  if(!listed_key(options.case_id_key))return;
  const auto name=duplicate_key(options.case_id_key);if(!name)return;
  fail("DuplicateError",phase,"group_by keys contained duplicate output name '"+*name+"'. It's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '"+std::string(statistics?"group_by":"join")+"' <---\n"+literal_sort_plan(source,options,time)+
       contexts(statistics?std::vector<std::string>{"group_by"}:std::vector<std::string>{"group_by","filter","select","join right","join"}));
}
[[noreturn]] void literal_outside(const std::string& what){throw std::invalid_argument("Native Polars variants literal case key outside the observed domain: "+what);}
struct Group{std::string key,label;bool absent=false;std::vector<Cell> activities;};
struct Grouped{std::vector<Group> groups;std::size_t case_at=0;std::string case_dtype,activity_dtype;};
PolarsVariantsFilterOptions keyed(const PolarsVariantsFilterOptions& raw,const std::vector<std::string>& columns){
  auto options=raw;options.activity_key=keys::col_key(raw.activity_key,columns);
  if(collection(options.activity_key)&&!options.activity_key.children.empty())throw std::invalid_argument("Native Polars variants activity selector outside observed empty-list domain");
  return options;
}
Grouped grouped(const EventTable& source,const PolarsVariantsFilterOptions& raw,bool statistics,const std::string& phase,bool eager_source=false){
  const auto options=keyed(raw,source.columns);
  source.validate();if(source.index||source.dtypes.size()!=source.columns.size())throw std::invalid_argument("Native Polars variants require declared dtypes and no dataframe index");
  if(collection(options.case_id_key)&&!statistics){
    // Round CN: a list of present names or literals joins and then sorts by
    // its one list value.
    const auto& items=options.case_id_key.children;
    if(!items.empty()&&std::any_of(items.begin(),items.end(),[&](const EventAttribute& v){return text(v)&&std::find(source.columns.begin(),source.columns.end(),v.value)==source.columns.end();}))
      throw std::invalid_argument("Native Polars variants case selector outside observed empty-list domain");
    if(!items.empty())fail("ShapeError",phase,"sort expressions must have same length as DataFrame, got DataFrame height: "+std::to_string(source.rows.size())+" and Series length: 1");
    fail("InvalidOperationError",phase,"expected join keys/predicates\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nDF []; PROJECT */0 COLUMNS"+contexts({"join"}));
  }
  // Round CP: a list timestamp key is a Series literal too (built strictly
  // while Python builds the sort), sorted as its one value.
  const bool list_time=statistics&&(options.timestamp_key.container==AttributeContainerKind::kList||options.timestamp_key.container==AttributeContainerKind::kTuple);
  if(list_time)if(const auto error=detail::polars_lit::argument_literal_error(options.timestamp_key))fail("TypeError","apply",*error);
  // Round CT: the statistics sort by a None case key sorts by the timestamp
  // alone and group_by(None) keeps every row in one literal group (probe
  // ct/l1); the join by None fails in Python afterwards (top_k).
  const bool none_key=statistics&&null(options.case_id_key);
  if(collection(options.case_id_key)||integer(options.case_id_key)||none_key) {
    namespace lit=detail::polars_lit;
    const auto present=[&](const std::string& name){return std::find(source.columns.begin(),source.columns.end(),name)!=source.columns.end();};
    const bool dict_time=keys::dict_key(options.timestamp_key);
    const bool time_resolves=list_time||dict_time||(text(options.timestamp_key)&&present(options.timestamp_key.value));
    // Round CS: Python builds the sort's struct literal before any plan;
    // the group_by keys resolve before the activity column.
    if(dict_time)if(const auto error=lit::dict_literal_error(options.timestamp_key))fail("TypeError","apply",*error);
    const auto time=list_time?lit::literal_list_display(lit::argument_literal(options.timestamp_key)):sort_time(options,true);
    const auto case_sort=[&]{
      if(none_key)return std::string();
      return (collection(options.case_id_key)?lit::literal_list_display(lit::argument_literal(options.case_id_key)):lit::scalar_display(lit::value_of(options.case_id_key)))+", ";
    };
    const auto group_failure=[&](const std::string& name){
      fail("ColumnNotFoundError",phase,missing_prefix(name,source.columns)+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\nSORT BY ["+
           case_sort()+time+"]\n  "+frame_plan(source)+contexts({"group_by"}));
    };
    // Round CT: the sort's timestamp column resolves first, then the
    // group_by keys' str items, then the activity column (probe ct/l1).
    if(statistics&&text(options.timestamp_key)&&!present(options.timestamp_key.value))
      fail("ColumnNotFoundError",phase,missing_prefix(options.timestamp_key.value,source.columns)+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n"+
           frame_plan(source)+contexts({"sort","group_by"}));
    if(statistics&&time_resolves&&listed_key(options.case_id_key))
      for(const auto& item:options.case_id_key.children)if(text(item)&&!present(item.value))group_failure(item.value);
    if(time_resolves)duplicate_keys(source,options,true,phase,time);
    // Round CP: the statistics' group_by resolves the activity column before
    // the sort by the literal runs (CM bank 2026111550 case 159); round CT:
    // an int or None key too.
    if(statistics&&text(options.activity_key)&&!keys::by_name(options.activity_key.value)&&!present(options.activity_key.value)&&time_resolves)
      group_failure(options.activity_key.value);
    if(none_key){
      if(!time_resolves||!text(options.timestamp_key))literal_outside("a None key with a literal timestamp");
      const auto& act=options.activity_key;
      if(!text(act)||keys::by_name(act.value))literal_outside("the activity key");
      const auto time_at=static_cast<std::size_t>(std::find(source.columns.begin(),source.columns.end(),options.timestamp_key.value)-source.columns.begin());
      const auto activity_at=static_cast<std::size_t>(std::find(source.columns.begin(),source.columns.end(),act.value)-source.columns.begin());
      const auto time_kind=dtype(source.dtypes[time_at]);
      if(time_kind==Dtype::kEnum)literal_outside("an Enum timestamp with a None key");
      std::vector<std::size_t> order(source.rows.size());std::iota(order.begin(),order.end(),0);
      const auto earlier=[&](std::size_t a,std::size_t b){return compare(source.rows[a][time_at],source.rows[b][time_at],time_kind,nullptr)<0;};
      std::stable_sort(order.begin(),order.end(),earlier);
      // Only the first non-str activity matters (the variant's join); a tie
      // that mixes str and other cells before it has no single order.
      for(std::size_t at=0;at<order.size();){
        auto end=at+1;while(end<order.size()&&!earlier(order[at],order[end]))++end;
        bool strs=false,others=false;
        for(auto i=at;i<end;++i){const auto& c=source.rows[order[i]][activity_at];(c&&!null(*c)&&source.dtypes[activity_at]=="String"?strs:others)=true;}
        if(strs&&others)literal_outside("tied timestamps mixing activity types with a None key");
        if(others){
          bool nulls=false,values=false;
          for(auto i=at;i<end;++i){const auto& c=source.rows[order[i]][activity_at];(c&&!null(*c)?values:nulls)=true;}
          if(nulls&&values)literal_outside("tied timestamps mixing null and other activities with a None key");
          break;
        }
        at=end;
      }
      Grouped result;result.activity_dtype=source.dtypes[activity_at];
      if(order.empty())return result;
      Group group;group.key="literal";group.absent=true;
      for(const auto i:order)group.activities.push_back(source.rows[i][activity_at]);
      result.groups.push_back(std::move(group));return result;
    }
    // Round CS: one row survives the sort; each case id becomes a dict key
    // in Python, which a struct or list literal is not (CK bank 2026111671
    // case 425).
    if(source.rows.size()==1&&time_resolves){
      const auto items=key_items(options.case_id_key);
      // Round CU (probe u2): with several str items the statistics take each
      // group row's second value as its variant: a str iterates to its
      // characters, any other value fails to iterate.
      if(items.size()!=1&&statistics&&std::all_of(items.begin(),items.end(),[](const EventAttribute& v){return text(v);})){
        const auto& act=options.activity_key;
        if(!text(act)||keys::by_name(act.value)||!present(act.value))literal_outside("the activity key");
        const auto at=static_cast<std::size_t>(std::find(source.columns.begin(),source.columns.end(),items[1].value)-source.columns.begin());
        if(at>=source.columns.size())literal_outside("a missing second key item");
        const auto& cell=source.rows.front()[at];const auto& dt=source.dtypes[at];
        std::string type;
        if(!cell||null(*cell)||dt=="Null")type="NoneType";
        else if(dt=="String"||categorical_dtype(dt)||enum_dtype(dt))type.clear();
        else if(dtype(dt)==Dtype::kInteger)type="int";
        else if(dtype(dt)==Dtype::kFloat)type="float";
        else if(dt=="Boolean")type="bool";
        else if(dt=="Date")type="datetime.date";
        else if(dt.rfind("Datetime",0)==0)type="datetime.datetime";
        else literal_outside("a second key item of dtype "+dt);
        if(!type.empty())fail("TypeError","apply","'"+type+"' object is not iterable");
        Grouped result;result.activity_dtype="String";
        Group group;group.key="literal";
        const auto& text_value=cell->value;
        for(std::size_t i=0;i<text_value.size();){
          const auto lead=static_cast<unsigned char>(text_value[i]);
          const std::size_t width=lead<0x80?1:lead>=0xF0?4:lead>=0xE0?3:lead>=0xC0?2:1;
          group.activities.push_back(EventAttribute{"",text_value.substr(i,width)});i+=width;
        }
        result.groups.push_back(std::move(group));return result;
      }
      if(items.size()!=1)literal_outside("several key items on one row");
      const auto& item=items.front();
      if(text(item)){auto inner=raw;inner.case_id_key=item;return grouped(source,inner,statistics,phase,eager_source);}
      const auto& act=options.activity_key;
      if(!text(act)||keys::by_name(act.value)||!present(act.value))literal_outside("the activity key");
      if(item.container==AttributeContainerKind::kContainer)fail("TypeError","apply","unhashable type: 'dict'");
      if(listed_key(item))fail("TypeError","apply","unhashable type: 'list'");
      const auto activity_at=static_cast<std::size_t>(std::find(source.columns.begin(),source.columns.end(),act.value)-source.columns.begin());
      Grouped result;result.activity_dtype=source.dtypes[activity_at];
      Group group;group.key="literal";group.absent=null(item);group.activities.push_back(source.rows.front()[activity_at]);
      result.groups.push_back(std::move(group));return result;
    }
    if(source.rows.size()==1)literal_outside("the timestamp key on one row");
    fail("ShapeError",phase,"sort expressions must have same length as DataFrame, got DataFrame height: "+std::to_string(source.rows.size())+" and Series length: 1");
  }
  const auto position=[&](const std::string& key,bool activity=false){auto found=std::find(source.columns.begin(),source.columns.end(),key);if(found==source.columns.end())missing(source,key,activity,statistics,options,phase,eager_source||(!statistics&&phase=="apply"));return static_cast<std::size_t>(found-source.columns.begin());};
  const bool literal_time=statistics&&(keys::dict_key(options.timestamp_key)||list_time);
  if(literal_time&&!list_time)if(const auto error=detail::polars_lit::dict_literal_error(options.timestamp_key))fail("TypeError","apply",*error);
  const auto case_at=position(options.case_id_key.value),time_at=literal_time?case_at:position(statistics?options.timestamp_key.value:"time:timestamp");
  if(collection(options.activity_key)){
    if(statistics)return {};
    fail("ColumnNotFoundError",phase,missing_prefix("variant",{options.case_id_key.value})+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nAGGREGATE[maintain_order: true]\n  [] BY [col("+quote(options.case_id_key.value)+")]\n  FROM\n  SORT BY [col("+quote(options.case_id_key.value)+"), col(\"time:timestamp\")]\n    "+frame_plan(source)+contexts({"select","join right","join"}));
  }
  const auto activity_at=position(options.activity_key.value,true);const auto case_kind=dtype(source.dtypes[case_at]),time_kind=dtype(source.dtypes[time_at]);
  // Aggregating the case column by itself names it twice (round BX).
  if(statistics&&activity_at==case_at)fail("DuplicateError",phase,"column with name '"+options.case_id_key.value+"' has more than one occurrence\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n"+sort_plan(source,options,statistics)+contexts({"group_by"}));
  if(literal_time&&source.rows.size()!=1)fail("ShapeError",phase,"sort expressions must have same length as DataFrame, got DataFrame height: "+std::to_string(source.rows.size())+" and Series length: 1");
  const auto dictionary=[&](std::size_t at,Dtype kind)->std::optional<EnumRanks>{
    if(kind!=Dtype::kEnum)return std::nullopt;
    if(source.categorical_dtypes.empty()||!source.categorical_dtypes[at])throw std::invalid_argument("Native Polars variants require an Enum dictionary for selected sort column: "+source.columns[at]);
    EnumRanks ranks;const auto& labels=source.categorical_dtypes[at]->categories;ranks.reserve(labels.size());for(std::size_t i=0;i<labels.size();++i)ranks.emplace(labels[i].value,i);
    for(const auto& row:source.rows)if(row[at]&&!null(*row[at])&&!ranks.contains(row[at]->value))throw std::invalid_argument("Native Polars variants Enum cell absent from selected dictionary: "+source.columns[at]);return ranks;
  };
  const auto case_ranks=dictionary(case_at,case_kind),time_ranks=dictionary(time_at,time_kind);
  std::vector<std::size_t> order(source.rows.size());std::iota(order.begin(),order.end(),0);
  // Polars' unstable multi-column sort leaves equal rows in its own order (round BX).
  polars_sort::sort_by_keys(order,[&](auto a,auto b){const auto bycase=compare(source.rows[a][case_at],source.rows[b][case_at],case_kind,case_ranks?&*case_ranks:nullptr);return bycase?bycase<0:compare(source.rows[a][time_at],source.rows[b][time_at],time_kind,time_ranks?&*time_ranks:nullptr)<0;},source.dtypes[case_at],source.dtypes[time_at]);
  Grouped result;result.case_at=case_at;result.case_dtype=source.dtypes[case_at];result.activity_dtype=source.dtypes[activity_at];
  for(std::size_t at=0;at<order.size();){const auto key=group_key(source.rows[order[at]][case_at],source.dtypes[case_at]);auto end=at+1;while(end<order.size()&&group_key(source.rows[order[end]][case_at],source.dtypes[case_at])==key)++end;
    Group group;group.key=key;group.absent=key=="n";bool first=true;
    for(auto i=at;i<end;++i){const auto& cell=source.rows[order[i]][activity_at];group.activities.push_back(cell);if(!cell||null(*cell)||source.dtypes[activity_at]=="Null")continue;if(!first)group.label+=',';group.label+=string_cast(*cell,source.dtypes[activity_at]);first=false;}
    result.groups.push_back(std::move(group));at=end;
  }
  return result;
}
// Round CS: the join keeps every row or none, unless a one-row frame joins
// on its key columns. Round CP's empty-list rule (no admitted variant keeps
// every row on an anti join) held only for an empty frame: the literal-only
// selection has one row (CK bank 2026111671 case 369).
// Round CU (fresh CK 2026111813 case 490, probe u4): the positive (inner)
// join on a key of one literal appends the right side's literal as a
// `literal` column (Int32 for a small int, Null for None, a struct for a
// dict), whatever rows it keeps; the anti join appends nothing.
std::optional<detail::polars_group_keys::Item> joined_literal(const PolarsVariantsFilterOptions& options){
  const auto& key=options.case_id_key;
  if(!truth(options.positive))return std::nullopt;
  const bool listed=listed_key(key)&&key.children.size()==1&&!text(key.children.front());
  if(!listed&&!integer(key))return std::nullopt;
  try{return detail::polars_group_keys::literal_item(listed?key.children.front():key);}catch(const std::exception&){return std::nullopt;}
}
EventTable with_literal(EventTable table,const PolarsVariantsFilterOptions& options){
  const auto item=joined_literal(options);if(!item)return table;
  table.columns.push_back("literal");table.dtypes.push_back(item->dtype);
  if(!table.categorical_dtypes.empty())table.categorical_dtypes.push_back(std::nullopt);
  for(auto& row:table.rows)row.push_back(item->cell);
  return table;
}
std::optional<EventTable> literal_collect(const EventTable& source,const std::vector<std::string>& labels,const PolarsVariantsFilterOptions& options,const std::string& phase){
  const auto& key=options.case_id_key;
  if(!(listed_key(key)&&!key.children.empty())&&!integer(key))return std::nullopt;
  const auto items=key_items(key);
  const auto present=[&](const std::string& name){return std::find(source.columns.begin(),source.columns.end(),name)!=source.columns.end();};
  // A str item is a column, any other item (None among str items too) a
  // literal (CN bank 2026111672 case 2).
  const bool names=std::any_of(items.begin(),items.end(),[](const EventAttribute& v){return text(v);});
  const bool eager=phase=="apply";
  // Round CT: the sort by the literal and time:timestamp resolves its
  // column before the group_by keys (fresh CK bank 2026111713 case 380).
  if(!present("time:timestamp")){
    if(eager)literal_outside("no timestamp column");
    fail("ColumnNotFoundError",phase,missing_prefix("time:timestamp",source.columns)+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n"+frame_plan(source)+
         contexts({"sort","group_by","filter","select","join right","join"}));
  }
  // Round CT: then the group_by keys' str items (probe ct/l2).
  for(const auto& item:items)if(text(item)&&!present(item.value)){
    if(eager)throw std::invalid_argument("Native Polars variants case selector outside observed empty-list domain");
    fail("ColumnNotFoundError",phase,missing_prefix(item.value,source.columns)+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n"+literal_sort_plan(source,options,"col(\"time:timestamp\")")+
         contexts({"group_by","filter","select","join right","join"}));
  }
  for(const auto& item:items)if(!text(item)&&!item.is_null&&real(item)&&std::isnan(number(item)))literal_outside("a NaN key");
  const auto shape=[&]{fail("ShapeError",phase,"sort expressions must have same length as DataFrame, got DataFrame height: "+std::to_string(source.rows.size())+" and Series length: 1");};
  if(eager&&source.rows.size()!=1)shape();
  duplicate_keys(source,options,false,phase,"col(\"time:timestamp\")");
  const auto act=keyed(options,source.columns).activity_key;
  if(!text(act)||keys::by_name(act.value))literal_outside("the activity key");
  if(!present(act.value)){
    if(eager&&!listed_key(key))literal_outside("a missing activity with an int key");
    if(eager)missing(source,act.value,true,false,options,phase,true);
    fail("ColumnNotFoundError",phase,missing_prefix(act.value,source.columns)+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n"+literal_sort_plan(source,options,"col(\"time:timestamp\")")+
         contexts({"group_by","filter","select","join right","join"}));
  }
  if(!labels.empty()&&source.rows.size()!=1)shape();
  bool matched=false;
  if(names){
    if(!labels.empty()){
      const auto& row=source.rows.front();
      bool absent=false;
      for(const auto& item:items){
        if(!text(item)){if(null(item))absent=true;continue;}
        const auto at=static_cast<std::size_t>(std::find(source.columns.begin(),source.columns.end(),item.value)-source.columns.begin());if(!row[at]||null(*row[at]))absent=true;
      }
      const auto at=static_cast<std::size_t>(std::find(source.columns.begin(),source.columns.end(),act.value)-source.columns.begin());
      std::string label;if(row[at]&&!null(*row[at])&&source.dtypes[at]!="Null")label=string_cast(*row[at],source.dtypes[at]);
      matched=!absent&&std::find(labels.begin(),labels.end(),label)!=labels.end();
    }
  } else {
    matched=!null(items.front());
  }
  if(matched==truth(options.positive))return with_literal(source,options);
  auto result=source;result.rows.clear();return with_literal(std::move(result),options);
}
EventTable collect(const EventTable& source,const std::vector<std::string>& labels,const PolarsVariantsFilterOptions& options,const std::string& phase){
  if(const auto result=literal_collect(source,labels,options,phase))return *result;
  const auto groups=grouped(source,options,false,phase);std::unordered_map<std::string,bool> matches;
  for(const auto& group:groups.groups)matches.emplace(group.key,!group.absent&&std::find(labels.begin(),labels.end(),group.label)!=labels.end());
  auto result=source;result.rows.clear();
  // The kept case ids join back in sorted case order. An inner join whose
  // source frame is no taller than that list yields the list's order, each
  // case followed by its rows.
  // The list holds a null case whose variant matches too, though a null key
  // never joins (round CD).
  std::vector<std::string> kept;std::size_t listed=0;
  for(const auto& group:groups.groups){if(matches.at(group.key))kept.push_back(group.key);if(group.absent&&std::find(labels.begin(),labels.end(),group.label)!=labels.end())++listed;}
  listed+=kept.size();
  // Round CT: a one-row NaN float case key joins nothing (probe ct/j1).
  if(truth(options.positive)&&detail::polars_group_keys::one_row_nan_join(source,groups.case_at,listed))return result;
  if(truth(options.positive)&&source.rows.size()<=listed){
    for(const auto& key:kept)for(const auto& row:source.rows)if(group_key(row[groups.case_at],groups.case_dtype)==key)result.rows.push_back(row);
    return result;
  }
  for(const auto& row:source.rows){const auto key=group_key(row[groups.case_at],groups.case_dtype);if(matches.at(key)==truth(options.positive))result.rows.push_back(row);}return result;
}
LazyEventTable lazy(const LazyEventTable& source,std::vector<std::string> labels,const PolarsVariantsFilterOptions& options){
  validate(options);
  auto columns=source.columns();auto dtypes=source.dtypes();auto categorical=source.categorical_dtypes();
  if(const auto item=joined_literal(options)){columns.push_back("literal");if(!dtypes.empty())dtypes.push_back(item->dtype);if(!categorical.empty())categorical.push_back(std::nullopt);}
  return LazyEventTable(columns,[source,labels=std::move(labels),options](const auto& consume){const auto result=collect(source.materialize(),labels,options,"collect");for(const auto& row:result.rows)consume(row);},source.attributes(),source.metadata(),std::nullopt,dtypes,source.index(),categorical);
}
bool py_equal(const Cell& a,const Cell& b){if(!a||null(*a))return !b||null(*b);if(!b||null(*b))return false;if((integer(*a)||boolean(*a))&&(integer(*b)||boolean(*b)))return exact(*a)==exact(*b);if((real(*a)||integer(*a)||boolean(*a))&&(real(*b)||integer(*b)||boolean(*b)))return number(*a)==number(*b);if(date(*a)&&date(*b))return compare(a,b,Dtype::kDate)==0;return text(*a)&&text(*b)?a->value==b->value:a==b;}
bool py_less(const Cell& a,const Cell& b){
  if(!a||null(*a)||!b||null(*b))fail("TypeError","apply","'<' not supported between instances of '"+std::string(!a||null(*a)?"NoneType":core_type(*a))+"' and '"+std::string(!b||null(*b)?"NoneType":core_type(*b))+"'");
  if((integer(*a)||boolean(*a))&&(integer(*b)||boolean(*b)))return exact(*a)<exact(*b);
  if((real(*a)||integer(*a)||boolean(*a))&&(real(*b)||integer(*b)||boolean(*b)))return number(*a)<number(*b);
  if(text(*a)&&text(*b))return a->value<b->value;
  if(date(*a)&&date(*b))return compare(a,b,Dtype::kDate)<0;
  fail("TypeError","apply","'<' not supported between instances of '"+core_type(*a)+"' and '"+core_type(*b)+"'");
}
bool tuple_less(const std::vector<Cell>& a,const std::vector<Cell>& b){for(std::size_t i=0;i<std::min(a.size(),b.size());++i){if(py_equal(a[i],b[i]))continue;return py_less(a[i],b[i]);}return a.size()<b.size();}
struct Variant{std::vector<Cell> activities;std::size_t count=0;};
// CPython's short-list reverse sort observes this comparison order before
// joining tuple values; it matters for the retained mixed None/scalar errors.
void sort_variants(std::vector<Variant>& values){
  const auto less_variant=[](const Variant& a,const Variant& b){return a.count!=b.count?a.count<b.count:tuple_less(a.activities,b.activities);};
  if(values.size()>=64){std::stable_sort(values.begin(),values.end(),[&](const auto& a,const auto& b){return less_variant(b,a);});return;}
  std::reverse(values.begin(),values.end());
  if(values.size()>1){std::size_t run=2;const bool descending=less_variant(values[1],values[0]);
    if(descending){while(run<values.size()&&less_variant(values[run],values[run-1]))++run;std::reverse(values.begin(),values.begin()+static_cast<std::ptrdiff_t>(run));}
    else while(run<values.size()&&!less_variant(values[run],values[run-1]))++run;
    for(auto i=run;i<values.size();++i){auto pivot=std::move(values[i]);std::size_t left=0,right=i;while(left<right){const auto mid=left+(right-left)/2;if(less_variant(pivot,values[mid]))right=mid;else left=mid+1;}for(auto j=i;j>left;--j)values[j]=std::move(values[j-1]);values[left]=std::move(pivot);}
  }
  std::reverse(values.begin(),values.end());
}
std::vector<std::string> top_labels(const EventTable& source,const EventAttribute& maximum,const PolarsVariantsFilterOptions& options,bool eager){
  const auto groups=grouped(source,options,true,"apply",eager);if(eager)fail("AttributeError","apply","'DataFrame' object has no attribute 'collect'");
  // Round CM: the variant tuples are indexed only when some case exists.
  if(collection(keyed(options,source.columns).activity_key)&&!source.rows.empty())fail("IndexError","apply","tuple index out of range");
  std::vector<Variant> variants;std::unordered_map<std::string,std::size_t> positions;
  for(const auto& group:groups.groups){
    std::string key;for(const auto& cell:group.activities){const auto item=group_key(cell,groups.activity_dtype);key+=std::to_string(item.size())+':'+item;}
    // Round CH: each NaN reaches Python as its own float object, so a variant
    // tuple holding one equals no other (two NaN cases are two variants).
    if(std::any_of(group.activities.begin(),group.activities.end(),[](const Cell& c){return c&&!null(*c)&&real(*c)&&std::isnan(number(*c));}))key+="#nan"+std::to_string(variants.size());
    const auto [found,inserted]=positions.emplace(std::move(key),variants.size());if(inserted)variants.push_back({group.activities,1});else ++variants[found->second].count;
  }
  sort_variants(variants);
  const auto count=variants.size();std::size_t take=count;
  if(integer(maximum)||boolean(maximum)){const auto k=exact(maximum);if(k<TokenInteger(count)){const auto length=k<TokenInteger(0)?TokenInteger(count)+k:k;take=length<TokenInteger(0)?0:static_cast<std::size_t>(length);}}
  else if(real(maximum)){if(!(static_cast<double>(count)<number(maximum)))fail("TypeError","apply","slice indices must be integers or None or have an __index__ method");}
  else fail("TypeError","apply","'<' not supported between instances of 'int' and '"+core_type(maximum)+"'");
  std::vector<std::string> result;
  for(std::size_t i=0;i<take;++i){EventAttribute sequence;sequence.container=AttributeContainerKind::kTuple;for(const auto& cell:variants[i].activities){EventAttribute value;if(cell)value=*cell;else value.is_null=true;sequence.children.push_back(std::move(value));}result.push_back(join(sequence));}return result;
}
std::vector<PolarsVariantsFilterParameter> parameters(const EventAttribute& value){
  if(null(value))return {};
  if(value.container==AttributeContainerKind::kContainer){std::vector<PolarsVariantsFilterParameter> result;for(const auto& child:value.children)result.push_back({child.key,child,false});return result;}
  detail::check_parameter_carrier(value, [](const std::string& type, const std::string& message) { fail(type, "apply", message); });
  return {};
}
PolarsVariantsFilterOptions statistics_parameters(const std::vector<PolarsVariantsFilterParameter>& values) {
  // The public statistics companion unrolls keys in insertion order. Final
  // membership separately retains its own enum-preferred parameter lookup.
  PolarsVariantsFilterOptions options;
  for(const auto& value:values) {
    if(value.key=="pm4py:param:case_id_key")options.case_id_key=value.value;
    else if(value.key=="pm4py:param:activity_key")options.activity_key=value.value;
    else if(value.key=="pm4py:param:timestamp_key")options.timestamp_key=value.value;
    else if(value.key=="positive")options.positive=value.value;
  }
  return options;
}
[[noreturn]] void unavailable(){fail("AttributeError","apply","module 'pm4py.util.constants' has no attribute 'DEFAULT_NAME_KEY'");}
[[noreturn]] void missing_argument(const std::string& function,const std::string& argument){fail("TypeError","apply",function+"() missing 1 required positional argument: '"+argument+"'");}
}  // namespace

PolarsVariantsFilterOptions resolve_polars_variants_filter_parameters(const std::vector<PolarsVariantsFilterParameter>& values){PolarsVariantsFilterOptions options;for(bool enum_key:{false,true})for(const auto& value:values){if(value.enum_key!=enum_key)continue;if(value.key=="pm4py:param:case_id_key")options.case_id_key=value.value;else if(value.key=="pm4py:param:activity_key")options.activity_key=value.value;else if(value.key=="pm4py:param:timestamp_key")options.timestamp_key=value.value;else if(value.key=="positive")options.positive=value.value;}return options;}
EventTable filter_polars_variants(const EventTable& source,const EventAttribute& values,const PolarsVariantsFilterOptions& options){validate(options);return collect(source,admitted(values),options,"apply");}
LazyEventTable filter_polars_variants(const LazyEventTable& source,const EventAttribute& values,const PolarsVariantsFilterOptions& options){validate(options);return lazy(source,admitted(values),options);}
EventTable filter_polars_variants_top_k(const EventTable& source,const EventAttribute& maximum,const PolarsVariantsFilterOptions& options){validate(options,true);(void)top_labels(source,maximum,options,true);throw std::logic_error("Unreachable eager Polars top-k");}
LazyEventTable filter_polars_variants_top_k(const LazyEventTable& source,const EventAttribute& maximum,const PolarsVariantsFilterOptions& options){
  validate(options,true);auto labels=top_labels(source.materialize(),maximum,options,false);
  validate(options);return lazy(source,std::move(labels),options);
}
EventTable filter_polars_variants_from_list(const EventTable&,const EventAttribute&,const PolarsVariantsFilterOptions&){unavailable();}
LazyEventTable filter_polars_variants_from_list(const LazyEventTable&,const EventAttribute&,const PolarsVariantsFilterOptions&){unavailable();}
EventTable auto_filter_polars_variants(const EventTable&,const PolarsVariantsFilterOptions&){unavailable();}
LazyEventTable auto_filter_polars_variants(const LazyEventTable&,const PolarsVariantsFilterOptions&){unavailable();}
EventTable filter_polars_variants_parameters(const EventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants(s,v,resolve_polars_variants_filter_parameters(p));}
EventTable filter_polars_variants_parameter_value(const EventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_parameters(s,v,parameters(p));}
EventTable filter_polars_variants(const EventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply","admitted_variants");}
LazyEventTable filter_polars_variants_parameters(const LazyEventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants(s,v,resolve_polars_variants_filter_parameters(p));}
LazyEventTable filter_polars_variants_parameter_value(const LazyEventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_parameters(s,v,parameters(p));}
LazyEventTable filter_polars_variants(const LazyEventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply","admitted_variants");}
EventTable filter_polars_variants_from_list_parameters(const EventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants_from_list(s,v,resolve_polars_variants_filter_parameters(p));}
EventTable filter_polars_variants_from_list_parameter_value(const EventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_from_list_parameters(s,v,parameters(p));}
EventTable filter_polars_variants_from_list(const EventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply_from_variant_list","variants");}
LazyEventTable filter_polars_variants_from_list_parameters(const LazyEventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants_from_list(s,v,resolve_polars_variants_filter_parameters(p));}
LazyEventTable filter_polars_variants_from_list_parameter_value(const LazyEventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_from_list_parameters(s,v,parameters(p));}
LazyEventTable filter_polars_variants_from_list(const LazyEventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply_from_variant_list","variants");}
EventTable filter_polars_variants_top_k_parameters(const EventTable& source,const EventAttribute& maximum,const std::vector<PolarsVariantsFilterParameter>& values) {
  const auto statistics=statistics_parameters(values);validate(statistics,true);
  (void)top_labels(source,maximum,statistics,true);throw std::logic_error("Unreachable eager Polars top-k");
}
EventTable filter_polars_variants_top_k_parameter_value(const EventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_top_k_parameters(s,v,parameters(p));}
EventTable filter_polars_variants_top_k(const EventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("filter_variants_top_k","k");}
LazyEventTable filter_polars_variants_top_k_parameters(const LazyEventTable& source,const EventAttribute& maximum,const std::vector<PolarsVariantsFilterParameter>& values) {
  const auto statistics=statistics_parameters(values);validate(statistics,true);
  const auto labels=top_labels(source.materialize(),maximum,statistics,false);
  // Round CT: the join by a None case key fails once the labels exist.
  const auto filter=resolve_polars_variants_filter_parameters(values);
  if(null(filter.case_id_key))fail("ValueError","apply","must specify `on` OR `left_on` and `right_on`");
  return lazy(source,labels,filter);
}
LazyEventTable filter_polars_variants_top_k_parameter_value(const LazyEventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_top_k_parameters(s,v,parameters(p));}
LazyEventTable filter_polars_variants_top_k(const LazyEventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("filter_variants_top_k","k");}
EventTable auto_filter_polars_variants_parameters(const EventTable& s,const std::vector<PolarsVariantsFilterParameter>& p){return auto_filter_polars_variants(s,resolve_polars_variants_filter_parameters(p));}
EventTable auto_filter_polars_variants_parameter_value(const EventTable& s,const EventAttribute& p){return auto_filter_polars_variants_parameters(s,parameters(p));}
LazyEventTable auto_filter_polars_variants_parameters(const LazyEventTable& s,const std::vector<PolarsVariantsFilterParameter>& p){return auto_filter_polars_variants(s,resolve_polars_variants_filter_parameters(p));}
LazyEventTable auto_filter_polars_variants_parameter_value(const LazyEventTable& s,const EventAttribute& p){return auto_filter_polars_variants_parameters(s,parameters(p));}
}  // namespace minera
