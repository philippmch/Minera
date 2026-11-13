#include "minera/polars_timestamp_filter.hpp"
#include "polars_datetime.hpp"
#include "polars_column_hint.hpp"
#include "polars_list_literal.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <numeric>
#include <limits>
#include <optional>
#include <unordered_map>
#include <iomanip>
#include <sstream>
#include <locale>
#include <tuple>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {
enum class Operation { kEvents, kContained, kIntersecting, kAttribute };
[[noreturn]] void fail(std::string type, const std::string& phase, std::string message) {
  throw PolarsTimestampFilterError(std::move(type), phase, std::move(message));
}
bool null(const EventAttribute& v) { return v.is_null || is_native_na_attribute(v); }
bool collection(const EventAttribute& v) { return v.container != AttributeContainerKind::kScalar; }
bool boolean(const EventAttribute& v) { return v.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(v.typed_value); }
bool integer(const EventAttribute& v) { return v.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(v.typed_value) || std::holds_alternative<std::uint64_t>(v.typed_value); }
bool real(const EventAttribute& v) { return v.kind == AttributeKind::kFloat || std::holds_alternative<double>(v.typed_value); }
bool date(const EventAttribute& v) { return v.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(v.typed_value); }
bool text(const EventAttribute& v) { return !collection(v) && !null(v) && !boolean(v) && !integer(v) && !real(v) && !date(v); }
TokenInteger exact(const EventAttribute& v) {
  if (const auto* p = std::get_if<std::int64_t>(&v.typed_value)) return TokenInteger(*p);
  if (const auto* p = std::get_if<std::uint64_t>(&v.typed_value)) return TokenInteger(*p);
  if (const auto* p = std::get_if<bool>(&v.typed_value)) return TokenInteger(*p ? 1 : 0);
  if (boolean(v)) return TokenInteger(v.value == "true" || v.value == "1" ? 1 : 0);
  return TokenInteger(v.value);
}
double number(const EventAttribute& v) { return integer(v) || boolean(v) ? static_cast<double>(exact(v)) : native_attribute_number(v); }
bool truth(const EventAttribute& v) {
  if (null(v)) return false;
  if (collection(v)) return !v.children.empty();
  if (integer(v) || boolean(v)) return static_cast<bool>(exact(v));
  if (real(v)) return number(v) != 0;
  return !v.value.empty();
}
std::string python_type(const EventAttribute& v) {
  if (collection(v)) return v.container == AttributeContainerKind::kList ? "list" : v.container == AttributeContainerKind::kTuple ? "tuple" : v.container == AttributeContainerKind::kContainer ? "dict" : "set";
  if (null(v)) return "NoneType";
  if (boolean(v)) return "bool";
  if (integer(v)) return "int";
  if (real(v)) return "float";
  if (date(v)) return "datetime";
  return "str";
}
// The type name CPython's own TypeError and AttributeError messages print.
std::string core_type(const EventAttribute& v) {
  if (!collection(v) && !null(v) && date(v)) {
    const auto* native = std::get_if<NativeDate>(&v.typed_value);
    return (native ? native->value : v.value).size() == 10 ? "datetime.date" : "datetime.datetime";
  }
  return python_type(v);
}
std::string quote(const std::string& v) { return '"' + v + '"'; }
std::string debug_quote(const std::string& v) {
  std::string result = "\"";
  for (const auto c : v) {
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\0': result += "\\0"; break;
      default: result += c;
    }
  }
  return result + '"';
}
std::string real_text(double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  char buffer[128]; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (converted.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars numeric text");
  return std::string(buffer, converted.ptr);
}
std::string group_key(const std::optional<EventAttribute>& cell, const std::string& dtype) {
  if (!cell || null(*cell) || dtype == "Null") return "n";
  const auto& v = *cell;
  if (dtype == "String" || dtype == "Categorical" || dtype.starts_with("Categorical(") || dtype.starts_with("Enum(")) return "s" + v.value;
  if (dtype == "Float64") {
    const auto n = number(v);
    return std::isnan(n) ? "fNaN" : n == 0 ? "f0" : "f" + std::to_string(std::bit_cast<std::uint64_t>(n));
  }
  if (dtype == "Boolean" || dtype == "Int64" || dtype == "UInt64" || dtype == "UInt32" || dtype == "Int32") return "i" + exact(v).decimal();
  if (polars_datetime::temporal(dtype)) {
    const auto parsed = polars_datetime::ticks(std::get_if<NativeDate>(&v.typed_value) ? std::get<NativeDate>(v.typed_value).value : v.value, polars_datetime::column_unit(dtype));
    if (!parsed) throw std::invalid_argument("Native Polars case grouping requires valid declared temporal cells");
    return "t" + std::to_string(*parsed);
  }
  throw std::invalid_argument("Unsupported native Polars case dtype: " + dtype);
}
std::string literal(const EventAttribute& value, bool dynamic = false) {
  if (null(value)) return "null";
  if (collection(value)) {
    std::string out="[";
    for (std::size_t i=0;i<value.children.size();++i) { if(i)out+=", ";out+=literal(value.children[i]); }
    return out+"]";
  }
  if (boolean(value)) return truth(value)?"true":"false";
  if (integer(value)) return std::string(dynamic?"dyn int: ":"")+exact(value).decimal();
  if (real(value)) return std::string(dynamic?"dyn float: ":"")+real_text(number(value));
  return quote(value.value);
}
std::string frame_plan(const EventTable& source) {
  std::string out="DF [";
  for(std::size_t i=0;i<std::min<std::size_t>(4,source.columns.size());++i){if(i)out+=", ";out+=quote(source.columns[i]);}
  if(source.columns.size()>4)out+=", ...";
  return out+"]; PROJECT */"+std::to_string(source.columns.size())+" COLUMNS";
}
std::string contexts(const std::vector<std::string>& nodes) {
  if(nodes.empty())return "";
  std::string out="\n\nThis error occurred with the following context stack:\n";
  for(std::size_t i=0;i<nodes.size();++i)out+="\t["+std::to_string(i+1)+"] '"+nodes[i]+"'\n";
  return out;
}
std::vector<std::string> nodes(Operation op,bool filter=false) {
  std::vector<std::string> out;
  if(op==Operation::kEvents){if(filter)out.push_back("filter' failed");return out;}
  if(filter)out.push_back("filter' failed");
  else if(op!=Operation::kAttribute)out.push_back("group_by");
  if(!filter&&op!=Operation::kAttribute)out.push_back("filter");
  out.push_back("select");if(op==Operation::kAttribute)out.push_back("unique");
  out.push_back("join right");out.push_back("join");return out;
}
std::string context_text(std::vector<std::string> items) {
  // The failed-node suffix belongs outside the diagnostic's quoted node name.
  auto out=contexts(items);std::string needle="'filter' failed'";std::size_t at=0;
  while((at=out.find(needle,at))!=std::string::npos){out.replace(at,needle.size(),"'filter' failed");at+=15;}
  return out;
}
std::string resolved(Operation op,const std::string& plan,const std::vector<std::string>& stack,bool filter=false) {
  return "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING "+
      std::string(op==Operation::kEvents?(filter?"'filter' failed":"THIS_NODE"):"'join'")+" <---\n"+plan+context_text(stack);
}
ParsedExactTimestamp bound(const EventAttribute& value) {
  if(date(value)) {
    const auto lexical=std::get_if<NativeDate>(&value.typed_value)?std::get<NativeDate>(value.typed_value).value:value.value;
    if(lexical.size()==10)fail("TypeError","apply","'tzinfo' is an invalid keyword argument for replace()");
    const auto parsed=parse_timestamp_exact(lexical);
    if(!parsed)throw std::invalid_argument("Native Polars timestamp bound requires valid typed datetime");
    auto result=*parsed;result.seconds+=std::int64_t(result.offset_minutes)*60;result.offset_minutes=0;result.timezone_aware=true;return result;
  }
  if(!text(value))fail("AttributeError","apply","'"+core_type(value)+"' object has no attribute 'replace'");
  const auto invalid=[&]()->ParsedExactTimestamp{fail("ValueError","apply","time data '"+value.value+"' does not match format '%Y-%m-%d %H:%M:%S'");};
  std::istringstream input(value.value);input.imbue(std::locale::classic());std::tm calendar{};
  calendar.tm_year=std::numeric_limits<int>::min();calendar.tm_mon=calendar.tm_mday=calendar.tm_hour=calendar.tm_min=calendar.tm_sec=-1;
  input>>std::get_time(&calendar,"%Y-%m-%d %H:%M:%S");
  if(input.fail()||calendar.tm_year==std::numeric_limits<int>::min()||calendar.tm_mon<0||calendar.tm_mday<0||calendar.tm_hour<0||calendar.tm_min<0||calendar.tm_sec<0)return invalid();
  if(input.peek()!=std::char_traits<char>::eof()){std::string remains;std::getline(input,remains,'\0');fail("ValueError","apply","unconverted data remains: "+remains);}
  std::ostringstream normalized;normalized.imbue(std::locale::classic());normalized<<std::setfill('0')<<std::setw(4)<<calendar.tm_year+1900<<'-'<<std::setw(2)<<calendar.tm_mon+1<<'-'<<std::setw(2)<<calendar.tm_mday<<'T'<<std::setw(2)<<calendar.tm_hour<<':'<<std::setw(2)<<calendar.tm_min<<':'<<std::setw(2)<<calendar.tm_sec<<'Z';
  const auto parsed=parse_timestamp_exact(normalized.str());return parsed?*parsed:invalid();
}
bool less(const ParsedExactTimestamp& a,const ParsedExactTimestamp& b){return std::tie(a.seconds,a.nanoseconds)<std::tie(b.seconds,b.nanoseconds);}
bool equal(const ParsedExactTimestamp& a,const ParsedExactTimestamp& b){return !less(a,b)&&!less(b,a);}
std::string bound_text(const ParsedExactTimestamp& v) {
  auto text=format_timestamp(v.seconds);if(text.ends_with('Z'))text.pop_back();if(text.ends_with(".000"))text.resize(text.size()-4);std::replace(text.begin(),text.end(),'T',' ');
  if(v.nanoseconds){auto fraction=std::to_string(1000000000+v.nanoseconds).substr(1);while(fraction.ends_with('0'))fraction.pop_back();text+='.'+fraction;}
  return text+" UTC";
}
void selector(const EventAttribute& value) {
  if(!text(value)&&!collection(value))fail("TypeError","apply","invalid input for `col`\n\nExpected `str` or `DataType`, got '"+python_type(value)+"'.");
  if(collection(value)&&!value.children.empty())throw std::invalid_argument("Native Polars timestamp selector collection outside observed empty-list domain");
}
struct Query {Operation op;ParsedExactTimestamp low,high;PolarsTimestampFilterOptions options;EventAttribute attribute,value;};
Query prepare(Operation op,const EventAttribute& low,const EventAttribute& high,const PolarsTimestampFilterOptions& options,const EventAttribute& attribute={},const EventAttribute& value={}) {
  Query q{op,bound(low),bound(high),options,attribute,value};
  if(op==Operation::kAttribute)selector(attribute);
  selector(options.timestamp_key);
  if(op!=Operation::kEvents&&null(options.case_id_key))fail("ValueError","apply","must specify `on` OR `left_on` and `right_on`");
  return q;
}
std::string compare_expr(const std::string& column,const std::string& op,const std::string& value) {return "[(col("+quote(column)+")) "+op+" ("+value+")]";}
std::string combined(const std::string& a,const std::string& op,const std::string& b){return "[("+a+") "+op+" ("+b+")]";}
std::string time_expr(const Query& q) {
  const auto lo=bound_text(q.low),hi=bound_text(q.high);const auto& time=q.options.timestamp_key.value;
  if(q.op==Operation::kContained)return combined(compare_expr("first_timestamp",">=",lo),"&",compare_expr("last_timestamp","<=",hi));
  if(q.op==Operation::kIntersecting){
    const auto cover=combined(compare_expr("first_timestamp","<",lo),"&",compare_expr("last_timestamp",">",hi));
    if(equal(q.low,q.high))return cover;
    return combined(combined(combined(compare_expr("first_timestamp",">",lo),"&",compare_expr("first_timestamp","<",hi)),"|",combined(compare_expr("last_timestamp",">",lo),"&",compare_expr("last_timestamp","<",hi))),"|",cover);
  }
  if(equal(q.low,q.high))return compare_expr(time,"==",lo);
  return combined(compare_expr(time,">=",lo),"&",compare_expr(time,"<=",hi));
}
std::string whole_expr(const Query& q) {
  const auto time=time_expr(q);return q.op==Operation::kAttribute?combined(compare_expr(q.attribute.value,"==",literal(q.value,true)),"&",time):time;
}
std::string agg_plan(const EventTable& source,const Query& q) {
  return "AGGREGATE[maintain_order: false]\n  [col("+quote(q.options.timestamp_key.value)+").min().alias(\"first_timestamp\"), col("+quote(q.options.timestamp_key.value)+").max().alias(\"last_timestamp\")] BY [col("+quote(q.options.case_id_key.value)+")]\n  FROM\n  "+frame_plan(source);
}
std::string attribute_plan(const EventTable& source,const Query& q) {return "FILTER col("+quote(q.attribute.value)+") == "+literal(q.value)+"\nFROM\n  "+frame_plan(source);}
std::string indent(std::string value,const std::string& prefix){std::string out=prefix;for(const char c:value){out+=c;if(c=='\n')out+=prefix;}return out;}
std::string filtered_plan(const EventTable& source,const Query& q,bool upper) {
  std::string plan=q.op==Operation::kAttribute?attribute_plan(source,q):frame_plan(source);
  plan="FILTER col("+quote(q.options.timestamp_key.value)+") >= "+bound_text(q.low)+"\nFROM\n"+indent(plan,"  ");
  if(upper)plan="FILTER col("+quote(q.options.timestamp_key.value)+") <= "+bound_text(q.high)+"\nFROM\n"+indent(plan,"  ");return plan;
}
std::string missing_prefix(const std::string& key,const std::vector<std::string>& columns) {
  std::string out="unable to find column "+debug_quote(key)+"; valid columns: [";
  for(std::size_t i=0;i<columns.size();++i){if(i)out+=", ";out+=debug_quote(columns[i]);}out+=']';
  if (const auto hint = detail::polars_column_hint(key, columns)) out += "\n\nDid you mean " + debug_quote(*hint) + '?';
  return out;
}
[[noreturn]] void missing(const EventTable& source,const std::string& key,const Query& q,const std::string& phase,bool case_key=false) {
  auto plan=case_key&&q.op==Operation::kAttribute?filtered_plan(source,q,true):frame_plan(source);
  fail("ColumnNotFoundError",phase,missing_prefix(key,source.columns)+resolved(q.op,plan,nodes(q.op)));
}
[[noreturn]] void empty_selector(const EventTable& source,const Query& q,const std::string& phase,bool time) {
  if(!time||q.op==Operation::kEvents||q.op==Operation::kAttribute)
    fail("ComputeError",phase,"The predicate expanded to zero expressions. This may for example be caused by a regex not matching column names or a column dtype match not hitting any dtypes in the DataFrame"+resolved(q.op,frame_plan(source),nodes(q.op)));
  fail("ColumnNotFoundError",phase,missing_prefix("first_timestamp",{q.options.case_id_key.value})+resolved(q.op,"AGGREGATE[maintain_order: false]\n  [] BY [col("+quote(q.options.case_id_key.value)+")]\n  FROM\n  "+frame_plan(source),{"select","join right","join"}));
}
std::string intersect_plan_expr(const Query& q) {
  const auto a="col(\"first_timestamp\")",b="col(\"last_timestamp\")";const auto lo=bound_text(q.low),hi=bound_text(q.high);
  return "((("+std::string(a)+" > "+lo+") & ("+a+" < "+hi+")) | (("+b+" > "+lo+") & ("+b+" < "+hi+"))) | (("+a+" < "+lo+") & ("+b+" > "+hi+"))";
}
void time_dtype(const EventTable& source,std::size_t at,const Query& q,const std::string& phase) {
  const auto& dtype=source.dtypes[at];
  if(dtype=="String"||dtype=="Categorical"||dtype.starts_with("Enum(")) {
    std::string message="cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)";
    if(q.op==Operation::kEvents&&phase=="apply")fail("InvalidOperationError",phase,message);
    std::string plan;
    if(q.op==Operation::kContained)plan="FILTER col(\"first_timestamp\") >= "+bound_text(q.low)+"\nFROM\n"+indent(agg_plan(source,q),"  ");
    else if(q.op==Operation::kIntersecting)plan="FILTER "+intersect_plan_expr(q)+"\nFROM\n"+indent(agg_plan(source,q),"  ");
    else plan=filtered_plan(source,q,false);
    fail("InvalidOperationError",phase,message+resolved(q.op,plan,nodes(q.op,true),true));
  }
  if(dtype=="Boolean"||polars_datetime::naive(dtype)) {
    const auto column=q.op==Operation::kContained||q.op==Operation::kIntersecting?"first_timestamp":q.options.timestamp_key.value;
    const bool eq=(q.op==Operation::kEvents||q.op==Operation::kAttribute)&&equal(q.low,q.high);
    const std::string cmp=eq?"==":q.op==Operation::kIntersecting?(equal(q.low,q.high)?"<":">"):">=";
    const auto expression=compare_expr(column,cmp,bound_text(q.low));const auto larger=whole_expr(q);
    std::string message="could not evaluate "+(eq?std::string{}:"'"+cmp+"' ")+"comparison between series '"+column+"' of dtype: "+(dtype=="Boolean"?"Boolean":"Datetime('"+polars_datetime::symbol(polars_datetime::column_unit(dtype))+"')")+" and series 'literal' of dtype: Datetime('μs', 'UTC')\n\nThis error occurred in the following expression:\n\t"+expression+'\n';
    if(larger!=expression)message+="while evaluating this larger expression:\n\t"+larger+'\n';fail("SchemaError",phase,message);
  }
  if(!polars_datetime::temporal(dtype)&&dtype!="Null"&&dtype!="Int64"&&dtype!="UInt64"&&dtype!="Float64")throw std::invalid_argument("Unsupported native Polars timestamp dtype: "+dtype);
}
void attribute_dtype(const EventTable& source,std::size_t at,const Query& q,const std::string& phase) {
  const auto& dt=source.dtypes[at];const bool string=dt=="String"||dt=="Categorical"||dt.starts_with("Categorical(")||dt.starts_with("Enum(");
  const bool numeric=dt=="Int64"||dt=="UInt64"||dt=="Float64";
  if((string&&(integer(q.value)||real(q.value)))||(numeric&&text(q.value))) {
    const auto type=string?(integer(q.value)?"i32":"f64"):dt=="Float64"?"f64":dt=="UInt64"?"u64":"i64";
    fail("ComputeError",phase,"cannot compare string with numeric type ("+std::string(type)+")\n\nThis error occurred in the following expression:\n\t"+compare_expr(q.attribute.value,"==",literal(q.value,true))+"\nwhile evaluating this larger expression:\n\t"+whole_expr(q)+'\n');
  }
}
// Resolving the attribute filter, the lowest node, rejects list and datetime
// values on a String column.
void attribute_resolution(const EventTable& source,std::size_t at,const Query& q,const std::string& phase) {
  const auto& dt=source.dtypes[at];const bool string=dt=="String"||dt=="Categorical"||dt.starts_with("Categorical(")||dt.starts_with("Enum(");
  if(!string)return;
  const auto& v=q.value;
  if(v.container==AttributeContainerKind::kList||v.container==AttributeContainerKind::kTuple) {
    const auto plan="FILTER col("+quote(q.attribute.value)+") == "+literal(v)+".cast(String)\nFROM\n  "+frame_plan(source);
    fail("InvalidOperationError",phase,"cannot cast List type (inner: '"+detail::polars_list_literal(v).inner+"', to: 'String')"+resolved(q.op,plan,nodes(q.op,true),true));
  }
  if(v.container==AttributeContainerKind::kScalar&&date(v)) {
    const auto* native=std::get_if<NativeDate>(&v.typed_value);auto text=native?native->value:v.value;
    if(text.size()>10)text[10]=' ';
    text=text.size()>19?text.substr(0,19)+" UTC":text;
    const auto plan="FILTER col("+quote(q.attribute.value)+") == "+text+"\nFROM\n  "+frame_plan(source);
    fail("InvalidOperationError",phase,"cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)"+resolved(q.op,plan,nodes(q.op,true),true));
  }
}
ParsedExactTimestamp physical_instant(std::int64_t micros) {
  auto seconds=micros/1000000;auto fraction=micros%1000000;
  if(fraction<0){--seconds;fraction+=1000000;}
  return ParsedExactTimestamp{seconds,static_cast<std::uint32_t>(fraction*1000),true,0};
}
struct Instant { ParsedExactTimestamp temporal; std::optional<double> physical_float; };
double physical_bound(const ParsedExactTimestamp& value) {
  return static_cast<double>(TokenInteger(value.seconds)*TokenInteger(1000000)+TokenInteger(value.nanoseconds/1000));
}
bool less(const Instant& a,const Instant& b) {
  if(a.physical_float||b.physical_float) {
    const auto left=a.physical_float?*a.physical_float:physical_bound(a.temporal);
    const auto right=b.physical_float?*b.physical_float:physical_bound(b.temporal);
    if(std::isnan(left)||std::isnan(right))return !std::isnan(left)&&std::isnan(right);
    return left<right;
  }
  return less(a.temporal,b.temporal);
}
bool less(const Instant& a,const ParsedExactTimestamp& b){return less(a,Instant{b,std::nullopt});}
bool less(const ParsedExactTimestamp& a,const Instant& b){return less(Instant{a,std::nullopt},b);}
std::optional<Instant> instant(const std::optional<EventAttribute>& cell,const std::string& dtype) {
  if(!cell||null(*cell)||dtype=="Null")return std::nullopt;
  const auto& v=*cell;
  if(dtype=="Int64"||dtype=="UInt64")return Instant{physical_instant(static_cast<std::int64_t>(exact(v))),std::nullopt};
  if(dtype=="Float64")return Instant{{},number(v)};
  const auto parsed=parse_timestamp_exact(std::get_if<NativeDate>(&v.typed_value)?std::get<NativeDate>(v.typed_value).value:v.value);
  if(!parsed)throw std::invalid_argument("Native Polars timestamp filter requires valid declared temporal cells");
  // The column holds its unit (round BW).
  return Instant{polars_datetime::floor(*parsed,polars_datetime::column_unit(dtype)),std::nullopt};
}
bool attribute_match(const std::optional<EventAttribute>& cell,const std::string& dt,const EventAttribute& value) {
  if(!cell||null(*cell)||null(value)||dt=="Null")return false;
  const auto& v=*cell;
  if(dt=="String"||dt=="Categorical"||dt.starts_with("Categorical(")||dt.starts_with("Enum(")) {
    if(text(value))return v.value==value.value;
    if(boolean(value))return v.value==(truth(value)?"true":"false");
    if(value.container==AttributeContainerKind::kContainer){std::string s="{";for(std::size_t i=0;i<value.children.size();++i){if(i)s+=',';s+=literal(value.children[i]);}return v.value==s+'}';}
    return false;
  }
  if(integer(value)||real(value)||boolean(value)) {
    if(integer(v)&&integer(value))return exact(v)==exact(value);
    const auto a=number(v),b=number(value);return a==b||(std::isnan(a)&&std::isnan(b));
  }
  return false;
}
EventTable collect(const EventTable& source,const Query& q,const std::string& phase) {
  source.validate();if(source.index||source.dtypes.size()!=source.columns.size())throw std::invalid_argument("Native Polars timestamp filters require declared dtypes and no dataframe index");
  // Python builds the attribute value literal first.
  if(q.op==Operation::kAttribute&&(q.value.container==AttributeContainerKind::kList||q.value.container==AttributeContainerKind::kTuple))
    if(const auto error=detail::polars_list_literal(q.value).error)fail("TypeError",phase,*error);
  const auto position=[&](const std::string& name,bool case_key=false){auto p=std::find(source.columns.begin(),source.columns.end(),name);if(p==source.columns.end())missing(source,name,q,phase,case_key);return static_cast<std::size_t>(p-source.columns.begin());};
  const bool constant=q.op!=Operation::kEvents&&integer(q.options.case_id_key);
  if(q.op!=Operation::kEvents&&collection(q.options.case_id_key)) {
    if(!q.options.case_id_key.children.empty())throw std::invalid_argument("Native Polars timestamp case selector outside observed empty-list domain");
    fail("InvalidOperationError",phase,"expected join keys/predicates"+resolved(q.op,(q.op==Operation::kAttribute?"UNIQUE[maintain_order: false, keep_strategy: Any] BY None\n  ":"")+std::string("DF []; PROJECT */0 COLUMNS"),{"join"}));
  }
  std::optional<std::size_t> case_at;
  if(q.op!=Operation::kEvents&&!constant&&q.op!=Operation::kAttribute)case_at=position(q.options.case_id_key.value);
  std::optional<std::size_t> attr_at;
  if(q.op==Operation::kAttribute){if(collection(q.attribute))empty_selector(source,q,phase,false);attr_at=position(q.attribute.value);}
  if(collection(q.options.timestamp_key))empty_selector(source,q,phase,true);
  const auto time_at=position(q.options.timestamp_key.value);
  if(attr_at)attribute_resolution(source,*attr_at,q,phase);
  if(q.op==Operation::kAttribute&&!constant)case_at=position(q.options.case_id_key.value,true);
  auto result=source;result.rows.clear();
  // Lazy predicate simplification occurs before comparing the temporal schemas.
  if((q.op==Operation::kEvents||q.op==Operation::kAttribute)&&less(q.high,q.low))return result;
  if(attr_at)attribute_dtype(source,*attr_at,q,phase);
  time_dtype(source,time_at,q,phase);
  // The microsecond bounds meet the column's unit as Polars 1.44.1 compares
  // a Datetime column with a datetime literal (round BW): every comparison
  // floors the bound to a millisecond column's unit; on a nanosecond column
  // `<=` and `==` floor the cell to microseconds while `>` and `!=` compare
  // the exact nanoseconds (`>=` and `<` agree either way).
  const auto& time_dtype_name=source.dtypes[time_at];
  const bool temporal_time=polars_datetime::temporal(time_dtype_name);
  const auto time_unit=polars_datetime::column_unit(time_dtype_name);
  const auto floor_bound=[&](const ParsedExactTimestamp& v){return temporal_time?polars_datetime::floor(v,polars_datetime::comparison_unit(time_unit)):v;};
  const auto low=floor_bound(q.low),high=floor_bound(q.high);
  // The cell side of a `<=` comparison.
  const auto at_most=[&](const Instant& v){return temporal_time&&!v.physical_float?Instant{polars_datetime::floor(v.temporal,polars_datetime::comparison_unit(time_unit)),std::nullopt}:v;};
  if(constant){result.columns.push_back("literal");result.dtypes.push_back("Int32");if(!result.categorical_dtypes.empty())result.categorical_dtypes.push_back(std::nullopt);for(const auto& row:source.rows){auto kept=row;auto literal=q.options.case_id_key;literal.key="literal";kept.push_back(literal);result.rows.push_back(std::move(kept));}return result;}
  struct Group{bool absent=false,keep=false;std::optional<Instant> first,last;};
  std::vector<Group> groups;std::unordered_map<std::string,std::size_t> keys;std::vector<std::size_t> row_groups;
  for(const auto& row:source.rows){
    const auto time=instant(row[time_at],source.dtypes[time_at]);
    const bool matches=time&&!less(*time,low)&&!less(high,at_most(*time))&&(q.op!=Operation::kAttribute||attribute_match(row[*attr_at],source.dtypes[*attr_at],q.value));
    if(q.op==Operation::kEvents){if(matches)result.rows.push_back(row);continue;}
    const auto key=group_key(row[*case_at],source.dtypes[*case_at]);const auto [found,inserted]=keys.emplace(key,groups.size());
    if(inserted){groups.push_back({});groups.back().absent=key=="n";}
    auto& group=groups[found->second];row_groups.push_back(found->second);
    if(q.op==Operation::kAttribute)group.keep=group.keep||matches;
    if(time){if(!group.first||less(*time,*group.first))group.first=time;if(!group.last||less(*group.last,*time))group.last=time;}
  }
  if(q.op==Operation::kContained||q.op==Operation::kIntersecting)for(auto& g:groups)if(g.first&&g.last){
    if(q.op==Operation::kContained)g.keep=!less(*g.first,low)&&!less(high,at_most(*g.last));
    else g.keep=(less(low,*g.first)&&less(*g.first,high))||(less(low,*g.last)&&less(*g.last,high))||(less(*g.first,low)&&less(high,*g.last));
  }
  if(q.op!=Operation::kEvents)for(std::size_t i=0;i<source.rows.size();++i){const auto& g=groups[row_groups[i]];if(g.keep&&!g.absent)result.rows.push_back(source.rows[i]);}
  return result;
}
EventTable eager(const EventTable& source,Operation op,const EventAttribute& lower,const EventAttribute& upper,const PolarsTimestampFilterOptions& options,const EventAttribute& attribute={},const EventAttribute& value={}) {return collect(source,prepare(op,lower,upper,options,attribute,value),"apply");}
LazyEventTable lazy(const LazyEventTable& source,Operation op,const EventAttribute& lower,const EventAttribute& upper,const PolarsTimestampFilterOptions& options,const EventAttribute& attribute={},const EventAttribute& value={}) {
  const auto q=prepare(op,lower,upper,options,attribute,value);auto columns=source.columns(),dtypes=source.dtypes();auto categories=source.categorical_dtypes();
  if(op!=Operation::kEvents&&integer(options.case_id_key)){columns.push_back("literal");if(!dtypes.empty())dtypes.push_back("Int32");if(!categories.empty())categories.push_back(std::nullopt);}
  return LazyEventTable(std::move(columns),[source,q](const auto& consume){const auto filtered=collect(source.materialize(),q,"collect");for(const auto& row:filtered.rows)consume(row);},source.attributes(),source.metadata(),std::nullopt,std::move(dtypes),source.index(),std::move(categories));
}
std::vector<PolarsTimestampFilterParameter> parameters(const EventAttribute& value) {
  if(null(value))return {};
  if(value.container==AttributeContainerKind::kContainer){std::vector<PolarsTimestampFilterParameter> result;for(const auto& child:value.children)result.push_back({child.key,child,false});return result;}
  detail::check_parameter_carrier(value, [](const std::string& type, const std::string& message) { fail(type, "apply", message); });
  return {};
}
}  // namespace

PolarsTimestampFilterOptions resolve_polars_timestamp_filter_parameters(const std::vector<PolarsTimestampFilterParameter>& values){PolarsTimestampFilterOptions options;for(bool enum_key:{false,true})for(const auto& v:values){if(v.enum_key!=enum_key)continue;if(v.key=="pm4py:param:timestamp_key")options.timestamp_key=v.value;else if(v.key=="pm4py:param:case_id_key")options.case_id_key=v.value;}return options;}
EventTable filter_polars_timestamp_events(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return eager(s,Operation::kEvents,l,h,o);}
EventTable filter_polars_timestamp_events_parameters(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_events(s,l,h,resolve_polars_timestamp_filter_parameters(p));}
EventTable filter_polars_timestamp_events_parameter_value(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_events_parameters(s,l,h,parameters(p));}
LazyEventTable filter_polars_timestamp_events(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return lazy(s,Operation::kEvents,l,h,o);}
LazyEventTable filter_polars_timestamp_events_parameters(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_events(s,l,h,resolve_polars_timestamp_filter_parameters(p));}
LazyEventTable filter_polars_timestamp_events_parameter_value(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_events_parameters(s,l,h,parameters(p));}
EventTable filter_polars_timestamp_contained(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return eager(s,Operation::kContained,l,h,o);}
EventTable filter_polars_timestamp_contained_parameters(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_contained(s,l,h,resolve_polars_timestamp_filter_parameters(p));}
EventTable filter_polars_timestamp_contained_parameter_value(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_contained_parameters(s,l,h,parameters(p));}
LazyEventTable filter_polars_timestamp_contained(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return lazy(s,Operation::kContained,l,h,o);}
LazyEventTable filter_polars_timestamp_contained_parameters(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_contained(s,l,h,resolve_polars_timestamp_filter_parameters(p));}
LazyEventTable filter_polars_timestamp_contained_parameter_value(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_contained_parameters(s,l,h,parameters(p));}
EventTable filter_polars_timestamp_intersecting(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return eager(s,Operation::kIntersecting,l,h,o);}
EventTable filter_polars_timestamp_intersecting_parameters(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_intersecting(s,l,h,resolve_polars_timestamp_filter_parameters(p));}
EventTable filter_polars_timestamp_intersecting_parameter_value(const EventTable& s,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_intersecting_parameters(s,l,h,parameters(p));}
LazyEventTable filter_polars_timestamp_intersecting(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return lazy(s,Operation::kIntersecting,l,h,o);}
LazyEventTable filter_polars_timestamp_intersecting_parameters(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_intersecting(s,l,h,resolve_polars_timestamp_filter_parameters(p));}
LazyEventTable filter_polars_timestamp_intersecting_parameter_value(const LazyEventTable& s,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_intersecting_parameters(s,l,h,parameters(p));}
EventTable filter_polars_timestamp_attribute(const EventTable& s,const EventAttribute& a,const EventAttribute& v,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return eager(s,Operation::kAttribute,l,h,o,a,v);}
EventTable filter_polars_timestamp_attribute_parameters(const EventTable& s,const EventAttribute& a,const EventAttribute& v,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_attribute(s,a,v,l,h,resolve_polars_timestamp_filter_parameters(p));}
EventTable filter_polars_timestamp_attribute_parameter_value(const EventTable& s,const EventAttribute& a,const EventAttribute& v,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_attribute_parameters(s,a,v,l,h,parameters(p));}
LazyEventTable filter_polars_timestamp_attribute(const LazyEventTable& s,const EventAttribute& a,const EventAttribute& v,const EventAttribute& l,const EventAttribute& h,const PolarsTimestampFilterOptions& o){return lazy(s,Operation::kAttribute,l,h,o,a,v);}
LazyEventTable filter_polars_timestamp_attribute_parameters(const LazyEventTable& s,const EventAttribute& a,const EventAttribute& v,const EventAttribute& l,const EventAttribute& h,const std::vector<PolarsTimestampFilterParameter>& p){return filter_polars_timestamp_attribute(s,a,v,l,h,resolve_polars_timestamp_filter_parameters(p));}
LazyEventTable filter_polars_timestamp_attribute_parameter_value(const LazyEventTable& s,const EventAttribute& a,const EventAttribute& v,const EventAttribute& l,const EventAttribute& h,const EventAttribute& p){return filter_polars_timestamp_attribute_parameters(s,a,v,l,h,parameters(p));}
EventTable apply_polars_timestamp_filter(const EventTable&,const EventAttribute&){fail("Exception","apply","apply method not available for timestamp filter");}
EventTable auto_filter_polars_timestamp(const EventTable&,const EventAttribute&){fail("Exception","apply","apply_auto_filter method not available for timestamp filter");}
LazyEventTable apply_polars_timestamp_filter(const LazyEventTable&,const EventAttribute&){fail("Exception","apply","apply method not available for timestamp filter");}
LazyEventTable auto_filter_polars_timestamp(const LazyEventTable&,const EventAttribute&){fail("Exception","apply","apply_auto_filter method not available for timestamp filter");}
}  // namespace minera
