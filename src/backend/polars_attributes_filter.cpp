#include "minera/polars_attributes_filter.hpp"
#include "polars_column_hint.hpp"
#include "polars_float_text.hpp"
#include "polars_list_literal.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {
using Op = PolarsAttributesFilterOperation;
[[noreturn]] void fail(std::string type, const std::string& phase, std::string message) {
  throw PolarsAttributesFilterError(std::move(type), phase, std::move(message));
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
  if (dtype == "Date" || dtype.starts_with("Datetime(time_unit='us', time_zone=")) {
    const auto parsed = parse_timestamp_microseconds(std::get_if<NativeDate>(&v.typed_value) ? std::get<NativeDate>(v.typed_value).value : v.value);
    if (!parsed) throw std::invalid_argument("Native Polars case grouping requires valid declared temporal cells");
    return "t" + std::to_string(*parsed);
  }
  throw std::invalid_argument("Unsupported native Polars case dtype: " + dtype);
}
std::string frame_plan(const EventTable& source) {
  std::string out="DF [";
  for(std::size_t i=0;i<std::min<std::size_t>(4,source.columns.size());++i){if(i)out+=", ";out+=quote(source.columns[i]);}
  if(source.columns.size()>4)out+=", ...";
  return out+"]; PROJECT */"+std::to_string(source.columns.size())+" COLUMNS";
}
std::string phase(bool eager) { return eager ? "apply" : "collect"; }
bool traced(Op op) { return op==Op::kTraces || op==Op::kNumericTraces || op==Op::kOnValues; }
bool numeric(Op op) { return op==Op::kNumericTraces || op==Op::kNumericEvents; }
bool statistics(Op op) { return op==Op::kThreshold || op==Op::kTopActivities || op==Op::kRelative || op==Op::kCounts; }
std::string function_name(Op op) {
  switch(op) {
    case Op::kTraces:return "apply";case Op::kEvents:return "apply_events";
    case Op::kNumericTraces:return "apply_numeric";case Op::kNumericEvents:return "apply_numeric_events";
    case Op::kThreshold:return "filter_df_keeping_activ_exc_thresh";case Op::kTopActivities:return "filter_df_keeping_spno_activities";
    case Op::kOnValues:return "filter_df_on_attribute_values";case Op::kRelative:return "filter_df_relative_occurrence_event_attribute";
    default:return "get_attribute_values";
  }
}
EventAttribute nil() { EventAttribute v;v.is_null=true;return v; }
EventAttribute count_value(std::size_t n) { EventAttribute v{"",std::to_string(n),AttributeKind::kInt};v.typed_value=static_cast<std::uint64_t>(n);return v; }
std::string stack(const std::vector<std::string>& items) {
  if(items.empty())return "";
  std::string out="\n\nThis error occurred with the following context stack:\n";
  for(std::size_t i=0;i<items.size();++i)out+="\t["+std::to_string(i+1)+"] "+items[i]+"\n";
  return out;
}
std::string resolved(std::string node,const std::string& plan,const std::vector<std::string>& nodes={}) {
  return "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING "+node+" <---\n"+plan+stack(nodes);
}
std::vector<std::string> join_stack(bool filter=false) {
  std::vector<std::string> out;if(filter)out.push_back("'filter' failed");
  out.insert(out.end(),{"'select'","'unique'","'join right'","'join'"});return out;
}
std::string filter_plan(const EventTable& table,const std::string& expression) {
  return "FILTER "+expression+"\nFROM\n  "+frame_plan(table);
}
std::string expression_literal(const EventAttribute& value,bool dynamic=false,bool decimal=false) {
  if(null(value))return "null";
  if(collection(value)) {
    // Polars prints the first two and the last item of a longer list.
    const auto& items=value.children;std::string out="[";
    for(std::size_t i=0;i<items.size();++i) {
      if(items.size()>3&&i==2){out+=", \xe2\x80\xa6 "+expression_literal(items.back());break;}
      if(i)out+=", ";out+=expression_literal(items[i]);
    }
    return out+"]";
  }
  if(boolean(value))return truth(value)?"true":"false";
  if(date(value)) {
    const auto* native=std::get_if<NativeDate>(&value.typed_value);auto text=native?native->value:value.value;
    if(text.size()>10)text[10]=' ';
    return text.size()>19?text.substr(0,19)+" UTC":text;
  }
  if(integer(value))return decimal?real_text(number(value))+ (real_text(number(value)).find('.')==std::string::npos?".0":"") : (dynamic?"dyn int: ":"")+exact(value).decimal();
  if(real(value)) {auto out=real_text(number(value));if(decimal && std::isfinite(number(value)) && out.find_first_of(".eE")==std::string::npos)out+=".0";return std::string(dynamic?"dyn float: ":"")+out;}
  return quote(value.value);
}
std::string membership_expr(const std::string& key,const EventAttribute& values,bool positive) {
  std::string operand;
  // Polars folds membership in an empty literal list to a false literal.
  if(positive&&collection(values)&&values.container!=AttributeContainerKind::kContainer&&values.children.empty())return "false.alias("+quote(key)+")";
  if(text(values))operand="col("+quote(values.value)+").implode()";
  else if(collection(values)) {
    if(values.container==AttributeContainerKind::kContainer) {
      EventAttribute keys=values;keys.container=AttributeContainerKind::kList;for(auto& v:keys.children)v=EventAttribute{"",v.key};operand=expression_literal(keys);
    } else operand=expression_literal(values);
  } else operand=expression_literal(values,true)+".implode()";
  return "col("+quote(key)+").is_in(["+operand+"])"+(positive?"":".not()");
}
std::string column_prefix(const EventTable& source,const std::string& key) {
  std::string out="unable to find column "+quote(key)+"; valid columns: [";
  for(std::size_t i=0;i<source.columns.size();++i){if(i)out+=", ";out+=debug_quote(source.columns[i]);}out+="]";
  if (const auto hint = detail::polars_column_hint(key, source.columns)) out += "\n\nDid you mean " + debug_quote(*hint) + '?';
  return out;
}
std::optional<std::size_t> index_of(const EventTable& table,const std::string& name) {
  auto p=std::find(table.columns.begin(),table.columns.end(),name);if(p==table.columns.end())return {};return std::distance(table.columns.begin(),p);
}
void col_selector(const EventAttribute& value) {
  if(!text(value)&&!collection(value))fail("TypeError","apply","invalid input for `col`\n\nExpected `str` or `DataType`, got '"+python_type(value)+"'.");
  if(collection(value))for(const auto& item:value.children)if(!text(item))fail("TypeError","apply","'"+python_type(item)+"' object is not an instance of 'str'");
  if(collection(value)&&!value.children.empty())throw std::invalid_argument("Native Polars attribute selector accepts only literal String or captured empty list");
}
PolarsAttributesFilterCall prepare(PolarsAttributesFilterCall call) {
  const auto op=call.operation;
  std::string required;
  if(op==Op::kTraces||op==Op::kEvents||op==Op::kOnValues){if(!call.values)required="values";}
  else if(numeric(op)){if(!call.lower)required="int1";else if(!call.upper)required="int2";}
  else if(op==Op::kThreshold){if(!call.threshold)required="thresh";}
  else if(op==Op::kRelative){if(!call.stake)required="min_relative_stake";}
  else if(op==Op::kCounts){if(!call.attribute)required="attribute_key";}
  if(!required.empty())fail("TypeError","apply",function_name(op)+"() missing 1 required positional argument: '"+required+"'");
  if(call.parameter_value && !null(*call.parameter_value)) {
    const auto& v=*call.parameter_value;
    if(collection(v)&&v.container==AttributeContainerKind::kContainer) {
      std::vector<PolarsAttributesFilterParameter> items;for(const auto& child:v.children)items.push_back({child.key,child,false});call.parameters=std::move(items);
    } else {
      detail::check_parameter_carrier(v,[](const std::string& type,const std::string& message){fail(type,"apply",message);});
      call.parameters=std::vector<PolarsAttributesFilterParameter>{};
    }
  }
  if(call.parameters) {
    const auto& values=*call.parameters;
    auto get=[&](const std::string& key)->const EventAttribute* {
      // Ordinary helpers use public enum precedence; relative forwards string
      // aliases into its counts helper, retaining the observed String override.
      const EventAttribute* plain=nullptr;const EventAttribute* enumerated=nullptr;
      for(const auto& p:values)if(p.key==key)(p.enum_key?enumerated:plain)=&p.value;
      return op==Op::kRelative && key=="pm4py:param:attribute_key" ? (plain?plain:enumerated) : (enumerated?enumerated:plain);
    };
    for(auto [name,target]:std::vector<std::pair<std::string,EventAttribute*>>{{"pm4py:param:attribute_key",&call.options.attribute_key},{"pm4py:param:activity_key",&call.options.activity_key},{"pm4py:param:case_id_key",&call.options.case_id_key},{"positive",&call.options.positive},{"decreasingFactor",&call.options.decreasing_factor}})if(auto v=get(name))*target=*v;
    for(auto [name,target]:std::vector<std::pair<std::string,std::optional<EventAttribute>*>>{{"keep_once_per_case",&call.options.keep_once_per_case},{"stream_filter_key1",&call.options.stream_filter_key1},{"stream_filter_value1",&call.options.stream_filter_value1},{"stream_filter_key2",&call.options.stream_filter_key2},{"stream_filter_value2",&call.options.stream_filter_value2}})if(auto v=get(name))*target=*v;
  }
  if(!statistics(op)) {
    col_selector(call.options.attribute_key);
    auto literal=[](const EventAttribute& v){if(collection(v)&&v.container!=AttributeContainerKind::kContainer)if(const auto error=detail::polars_list_literal(v).error)fail("TypeError","apply",*error);};
    if(call.values)literal(*call.values);
    if(numeric(op)) {
      // Python builds the bound literals, then each stream filter's column and literal.
      literal(*call.lower);literal(*call.upper);
      if(op==Op::kNumericTraces)for(auto [key,value]:std::vector<std::pair<const std::optional<EventAttribute>*,const std::optional<EventAttribute>*>>{{&call.options.stream_filter_key1,&call.options.stream_filter_value1},{&call.options.stream_filter_key2,&call.options.stream_filter_value2}}) {
        if(!*key||null(**key))continue;
        col_selector(**key);if(*value)literal(**value);
      }
    }
    if(traced(op)) {
      const auto& key=call.options.case_id_key;
      if(null(key))fail("ValueError","apply","must specify `on` OR `left_on` and `right_on`");
      if(!text(key)&&!collection(key)&&!integer(key))col_selector(key);
    }
  }
  return call;
}
std::string source_dtype(const EventTable& table,std::size_t column) {
  if(table.dtypes.size()!=table.columns.size())throw std::invalid_argument("Native Polars attribute filtering requires explicit source dtypes");return table.dtypes[column];
}
EventAttribute cell(const EventTable& source,std::size_t row,std::size_t col) { return source.rows[row][col]?*source.rows[row][col]:nil(); }
bool scalar_equal(const EventAttribute& a,const EventAttribute& b) {
  if(null(a)||null(b))return null(a)&&null(b);
  if((integer(a)||boolean(a))&&(integer(b)||boolean(b)))return exact(a)==exact(b);
  if((real(a)||integer(a)||boolean(a))&&(real(b)||integer(b)||boolean(b))) {
    if(real(a)!=real(b)) {
      const auto& f=real(a)?a:b;const auto& i=real(a)?b:a;const auto n=number(f);
      if(!std::isfinite(n)||std::floor(n)!=n)return false;
      char buffer[1024];const auto encoded=std::to_chars(buffer,buffer+sizeof(buffer),n,std::chars_format::fixed,0);
      if(encoded.ec!=std::errc{})return false;
      return exact(i)==TokenInteger(std::string_view(buffer,encoded.ptr-buffer));
    }
    const auto x=number(a),y=number(b);return x==y||(std::isnan(x)&&std::isnan(y));
  }
  return text(a)&&text(b)?a.value==b.value:date(a)&&date(b)&&a.value==b.value;
}
EventAttribute clean_value(EventAttribute v) {
  v.key.clear();v.first_class_role=FirstClassAttributeRole::kNone;v.first_class_state=FirstClassAttributeState::kMissing;v.first_class_source_key.clear();return v;
}
EventTable select_rows(const EventTable& source,const std::vector<bool>& selected) {
  EventTable out=source;out.rows.clear();if(out.index)out.index->labels.clear();
  for(std::size_t i=0;i<source.rows.size();++i)if(selected[i]){out.rows.push_back(source.rows[i]);if(out.index)out.index->labels.push_back(source.index->labels[i]);}return out;
}
struct Leaf { std::string column, op, dtype; EventAttribute value; };
struct Expr { std::optional<Leaf> leaf; std::string op; std::vector<Expr> kids; };
struct Predicate {
  PolarsAttributesFilterCall call;
  std::size_t column=0;
  std::optional<std::size_t> case_column, stream1, stream2;
  std::string dtype, case_dtype;
  bool literal_case=false;
  std::vector<EventAttribute> values;
  std::vector<Expr> chain;
  bool empty=false;
};
std::string list_dtype(const EventAttribute& query) {
  if(!collection(query)) {
    if(boolean(query))return "Boolean";
    if(integer(query))return "Unknown(Int("+exact(query).decimal()+"))";
    if(real(query))return "Unknown(Float)";
    if(null(query))return "Null";
    if(date(query))return detail::polars_literal_dtype(query);
    return "String";
  }
  if(query.container==AttributeContainerKind::kContainer)return "String";
  for(const auto& v:query.children)if(!null(v)) {
    if(collection(v))return "List("+list_dtype(v)+")";
    return detail::polars_literal_dtype(v);
  }
  return "Null";
}
std::string membership_type(std::string dtype) {
  if(dtype.starts_with("Datetime("))return "Datetime('\xce\xbcs', 'UTC')";
  if(dtype.starts_with("Categorical"))return "Categorical";
  if(dtype.starts_with("Enum("))return "Enum";
  return dtype;
}
int total_compare(double a,double b) {
  if(std::isnan(a))return std::isnan(b)?0:1;if(std::isnan(b))return -1;return a<b?-1:a>b?1:0;
}
// Numeric filters: Python builds the bound (and, for traces, stream equality)
// filters, Polars resolves them bottom-up, then evaluates the predicate
// pushdown merge of the chain.
Expr leaf_expr(const std::string& column,const std::string& op,const EventAttribute& value) { return Expr{Leaf{column,op,"",value},"",{}}; }
Expr binary(Expr a,const std::string& op,Expr b) { Expr out;out.op=op;out.kids.push_back(std::move(a));out.kids.push_back(std::move(b));return out; }
template<class F> void each_leaf(Expr& e,F&& f) { if(e.leaf)f(*e.leaf);else for(auto& k:e.kids)each_leaf(k,f); }
template<class F> void each_leaf(const Expr& e,F&& f) { if(e.leaf)f(*e.leaf);else for(const auto& k:e.kids)each_leaf(k,f); }
using detail::polars_dyn_float;
using detail::polars_plan_float;
std::string date_text(const EventAttribute& v) { const auto* native=std::get_if<NativeDate>(&v.typed_value);return native?native->value:v.value; }
std::int64_t date_micros(const EventAttribute& v) {
  const auto parsed=parse_timestamp_microseconds(date_text(v));
  if(!parsed)throw std::invalid_argument("Native Polars numeric bound requires a valid datetime");return *parsed;
}
std::string datetime_literal(const EventAttribute& v) {
  auto text=date_text(v);
  if(text.size()==19){text[10]=' ';return text;}
  if(text.size()==25&&text.ends_with("+00:00")){text[10]=' ';return text.substr(0,19)+" UTC";}
  throw std::invalid_argument("Native Polars numeric bound datetime outside observed domain");
}
bool numeric_dtype(const std::string& dtype) { return dtype=="Int64"||dtype=="Float64"; }
bool null_leaf(const Leaf& l) { return null(l.value); }
// The (lhs, rhs) operands Polars prints once the literal is coerced to the
// column; a None literal folds the comparison to a null column.
std::pair<std::string,std::string> operands(const Leaf& l,bool cast_list=false) {
  const auto column="col("+quote(l.column)+")";
  const auto& v=l.value;
  if(l.dtype=="String"||l.dtype.starts_with("Categorical")||l.dtype.starts_with("Enum(")) {
    if(collection(v))return {column,expression_literal(v)+(cast_list?".cast(String)":"")};
    if(date(v))return {column,datetime_literal(v)};
    if(boolean(v))return {column,quote(truth(v)?"true":"false")};
    if(integer(v))return {column,"dyn int: "+exact(v).decimal()};
    if(real(v))return {column,"dyn float: "+polars_dyn_float(number(v))};
    return {column,quote(v.value)};
  }
  if(!numeric_dtype(l.dtype))throw std::invalid_argument("Native Polars numeric filter supports String, Int64 and Float64 columns");
  if(collection(v))return {column,expression_literal(v)};
  if(text(v))return {column,quote(v.value)};
  if(l.dtype=="Int64") {
    if(real(v))return {column+".cast(Float64)",polars_plan_float(number(v))};
    if(date(v))return {column,std::to_string(date_micros(v))};
    return {column,exact(v).decimal()};
  }
  if(date(v))return {column,polars_plan_float(static_cast<double>(date_micros(v)))};
  return {column,polars_plan_float(number(v))};
}
std::string null_text(const Leaf& l) { return "null.repeat([col("+quote(l.column)+").len()])"; }
std::string plan_text(const Expr& e,const Leaf* cast=nullptr) {
  if(e.leaf){if(null_leaf(*e.leaf))return null_text(*e.leaf);const auto [a,b]=operands(*e.leaf,cast==&*e.leaf);return a+" "+e.leaf->op+" "+b;}
  auto wrap=[&](const Expr& k){const auto t=plan_text(k,cast);return k.leaf&&null_leaf(*k.leaf)?t:"("+t+")";};
  return wrap(e.kids[0])+" "+e.op+" "+wrap(e.kids[1]);
}
std::string error_text(const Expr& e) {
  if(e.leaf){if(null_leaf(*e.leaf))return null_text(*e.leaf);const auto [a,b]=operands(*e.leaf);return "[("+a+") "+e.leaf->op+" ("+b+")]";}
  return "[("+error_text(e.kids[0])+") "+e.op+" ("+error_text(e.kids[1])+")]";
}
std::string chain_plan(const EventTable& table,const std::vector<std::string>& filters) {
  std::string out,indent;
  for(auto p=filters.rbegin();p!=filters.rend();++p){out+=indent+"FILTER "+*p+"\n"+indent+"FROM\n";indent+="  ";}
  return out+indent+frame_plan(table);
}
const Expr* leaf_node(const Expr& e) { return e.leaf?&e:nullptr; }
// Predicate pushdown: walking down, a filter joins the pending predicates; a
// filter on a null column stays in place with the pending ones. Predicates
// combine per column name (bytewise order), lower filters to the left.
std::string predicate_column(const Expr& e) { std::string out;each_leaf(e,[&](const Leaf& l){if(out.empty())out=l.column;});return out; }
Expr combine(const std::vector<Expr>& items) {
  std::map<std::string,std::vector<const Expr*>> groups;
  for(const auto& item:items)groups[predicate_column(item)].push_back(&item);
  std::optional<Expr> out;
  for(const auto& [name,group]:groups) {
    Expr acc=*group[0];for(std::size_t i=1;i<group.size();++i)acc=binary(*group[i],"&",std::move(acc));
    out=out?binary(std::move(*out),"&",std::move(acc)):std::move(acc);
  }
  return *out;
}
std::vector<Expr> pushdown(const std::vector<Expr>& chain) {
  std::vector<Expr> nodes,pending;
  for(auto p=chain.rbegin();p!=chain.rend();++p) {
    if(p->leaf&&null_leaf(*p->leaf)) {
      std::vector<Expr> items{*p};items.insert(items.end(),pending.begin(),pending.end());
      nodes.push_back(combine(items));pending.clear();
    } else pending.push_back(*p);
  }
  if(!pending.empty())nodes.push_back(combine(pending));
  std::reverse(nodes.begin(),nodes.end());return nodes;
}
std::optional<std::pair<std::string,std::string>> leaf_error(const Leaf& l) {
  const auto& v=l.value;
  if(null(v))return {};
  if(l.dtype=="String") {
    if(integer(v)&&!boolean(v)){const auto n=exact(v);const bool small=n>=TokenInteger(std::numeric_limits<std::int32_t>::min())&&n<=TokenInteger(std::numeric_limits<std::int32_t>::max());return std::pair<std::string,std::string>{"ComputeError",std::string("cannot compare string with numeric type (")+(small?"i32":"i64")+")"};}
    if(real(v))return std::pair<std::string,std::string>{"ComputeError","cannot compare string with numeric type (f64)"};
    return {};
  }
  if(l.dtype.starts_with("Categorical")||l.dtype.starts_with("Enum(")) {
    if((integer(v)&&!boolean(v))||real(v))return std::pair<std::string,std::string>{"ComputeError","cannot compare categorical with "+std::string(real(v)?"f64":"i32")};
    return {};
  }
  if(text(v))return std::pair<std::string,std::string>{"ComputeError",std::string("cannot compare string with numeric type (")+(l.dtype=="Float64"?"f64":"i64")+")"};
  if(collection(v))return std::pair<std::string,std::string>{"SchemaError","could not evaluate "+(l.op=="=="?std::string():"'"+l.op+"' ")+"comparison between series '"+l.column+"' of dtype: "+l.dtype+" and series 'literal' of dtype: List("+detail::polars_list_literal(v).inner+")"};
  return {};
}
std::optional<std::pair<std::string,std::string>> first_error(const Expr& e,const Expr*& at) {
  if(e.leaf){auto error=leaf_error(*e.leaf);if(error)at=&e;return error;}
  for(const auto& k:e.kids)if(auto error=first_error(k,at))return error;
  return {};
}
// Kleene truth of a comparison: -1 null, 0 false, 1 true.
int leaf_truth(const Leaf& l,const EventAttribute& v) {
  if(null(v)||null(l.value))return -1;
  const auto& b=l.value;int c=0;
  if(l.dtype=="String"||l.dtype.starts_with("Categorical")||l.dtype.starts_with("Enum(")) {
    const auto rhs=boolean(b)?std::string(truth(b)?"true":"false"):b.value;c=v.value.compare(rhs);c=c<0?-1:c>0?1:0;
  } else if(l.dtype=="Int64"&&!real(b)) {
    const TokenInteger x=exact(v),y=date(b)?TokenInteger(date_micros(b)):exact(b);c=x<y?-1:y<x?1:0;
  } else c=total_compare(number(v),date(b)?static_cast<double>(date_micros(b)):number(b));
  const auto& op=l.op;
  return op==">="?c>=0:op=="<="?c<=0:op==">"?c>0:op=="<"?c<0:c==0;
}
// A literal comparable with another on the same coerced operand, as Polars'
// range simplification sees it.
std::optional<std::variant<std::string,TokenInteger,double>> range_value(const Leaf& l) {
  const auto& v=l.value;
  if(null(v)||collection(v))return {};
  if(l.dtype=="String"){if(text(v))return v.value;if(boolean(v))return std::string(truth(v)?"true":"false");return {};}
  if(text(v))return {};
  if(l.dtype=="Int64"&&!real(v))return date(v)?TokenInteger(date_micros(v)):exact(v);
  return date(v)?static_cast<double>(date_micros(v)):number(v);
}
bool empty_range(const Expr& node) {
  std::vector<const Leaf*> leaves;each_leaf(node,[&](const Leaf& l){leaves.push_back(&l);});
  if(node.op=="|")return false;
  for(const auto* lo:leaves)for(const auto* hi:leaves) {
    if(lo->op!=">="||hi->op!="<="||lo->column!=hi->column||null_leaf(*lo)||null_leaf(*hi)||operands(*lo).first!=operands(*hi).first)continue;
    const auto a=range_value(*lo),b=range_value(*hi);
    if(!a||!b||a->index()!=b->index())continue;
    if(const auto* x=std::get_if<double>(&*a)){if(total_compare(*x,std::get<double>(*b))>0)return true;}
    else if(*b<*a)return true;
  }
  return false;
}
int truth_of(const Expr& e,const EventTable& source,std::size_t row) {
  if(e.leaf)return leaf_truth(*e.leaf,cell(source,row,*index_of(source,e.leaf->column)));
  const int a=truth_of(e.kids[0],source,row),b=truth_of(e.kids[1],source,row);
  if(e.op=="&")return a==0||b==0?0:a<0||b<0?-1:1;
  return a==1||b==1?1:a<0||b<0?-1:0;
}
Predicate numeric_predicate(const EventTable& source,const PolarsAttributesFilterCall& call,bool eager) {
  const auto op=call.operation;const bool trace=op==Op::kNumericTraces;const auto& attr=call.options.attribute_key;
  // One level per Python filter call: the bounds, then each stream filter.
  std::vector<std::vector<Expr>> levels;std::vector<const EventAttribute*> keys{&attr};
  if(trace||truth(call.options.positive))levels.push_back({leaf_expr(attr.value,">=",*call.lower),leaf_expr(attr.value,"<=",*call.upper)});
  else levels.push_back({binary(leaf_expr(attr.value,"<",*call.lower),"|",leaf_expr(attr.value,">",*call.upper))});
  if(trace)for(auto [key,value]:std::vector<std::pair<const std::optional<EventAttribute>*,const std::optional<EventAttribute>*>>{{&call.options.stream_filter_key1,&call.options.stream_filter_value1},{&call.options.stream_filter_key2,&call.options.stream_filter_value2}}) {
    if(!*key||null(**key))continue;
    levels.push_back({leaf_expr((**key).value,"==",*value?**value:nil())});keys.push_back(&**key);
  }
  const auto fail_at=[&](std::size_t level,bool typed,const std::string& type,const std::string& message,const std::string& plan) {
    std::string suffix;
    if(!eager) {
      std::vector<std::string> nodes;if(typed)nodes.push_back("'filter' failed");
      if(trace) {
        for(std::size_t i=level+1;i<levels.size();++i)nodes.push_back("'filter'");
        nodes.insert(nodes.end(),{"'select'","'unique'","'join right'","'join'"});suffix=resolved("'join'",plan,nodes);
      } else suffix=resolved(typed?"'filter' failed":"THIS_NODE",plan,nodes);
    }
    fail(type,phase(eager),message+suffix);
  };
  Predicate p{call,0};std::vector<std::string> resolved_filters;
  for(std::size_t i=0;i<levels.size();++i) {
    const auto& key=*keys[i];
    if(collection(key))fail_at(i,false,"ComputeError","The predicate expanded to zero expressions. This may for example be caused by a regex not matching column names or a column dtype match not hitting any dtypes in the DataFrame",chain_plan(source,resolved_filters));
    const auto column=index_of(source,key.value);
    if(!column)fail_at(i,false,"ColumnNotFoundError",column_prefix(source,key.value),chain_plan(source,resolved_filters));
    if(i==0){p.column=*column;p.dtype=source_dtype(source,*column);}
    for(auto node:levels[i]) {
      const auto dtype=source_dtype(source,*column);
      each_leaf(node,[&](Leaf& l){l.dtype=dtype;});
      const Leaf* bad=nullptr;std::string message;
      each_leaf(node,[&](const Leaf& l) {
        if(bad||l.dtype!="String"||(!collection(l.value)&&!date(l.value)))return;
        bad=&l;
        message=collection(l.value)?"cannot cast List type (inner: '"+detail::polars_list_literal(l.value).inner+"', to: 'String')"
                                   :"cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)";
      });
      if(bad){auto with=resolved_filters;with.push_back(plan_text(node,bad));fail_at(i,true,"InvalidOperationError",message,chain_plan(source,with));}
      resolved_filters.push_back(plan_text(node));p.chain.push_back(std::move(node));
    }
  }
  if(trace) {
    const auto& role=call.options.case_id_key;
    if(collection(role))fail("InvalidOperationError",phase(eager),"expected join keys/predicates"+(eager?"":resolved("'join'","UNIQUE[maintain_order: false, keep_strategy: Any] BY None\n  DF []; PROJECT */0 COLUMNS",{"'join'"})));
    if(integer(role))p.literal_case=true;
    else {
      const auto c=index_of(source,role.value);
      if(!c)fail("ColumnNotFoundError",phase(eager),column_prefix(source,role.value)+(eager?"":resolved("'join'",chain_plan(source,resolved_filters),join_stack())));
      p.case_column=c;p.case_dtype=source_dtype(source,*c);
    }
  }
  for(const auto& node:pushdown(p.chain)) {
    // The optimizer drops a merged filter whose bounds leave an empty range.
    if(empty_range(node)){p.empty=true;continue;}
    const Expr* at=nullptr;
    if(const auto error=first_error(node,at))
      fail(error->first,phase(eager),error->second+"\n\nThis error occurred in the following expression:\n\t"+error_text(*at)+(node.leaf?"\n":"\nwhile evaluating this larger expression:\n\t"+error_text(node)+"\n"));
  }
  return p;
}
Predicate predicate(const EventTable& source,const PolarsAttributesFilterCall& call,bool eager) {
  if(numeric(call.operation))return numeric_predicate(source,call,eager);
  const auto op=call.operation;const bool trace=traced(op);const auto& attr=call.options.attribute_key;
  const auto suffix=[&](const std::string& plan,bool failed=false) {
    if(eager)return std::string{};
    return resolved(trace?"'join'":failed?"'filter' failed":"THIS_NODE",plan,trace?join_stack(failed):failed?std::vector<std::string>{"'filter' failed"}:std::vector<std::string>{});
  };
  if(collection(attr))fail("ComputeError",phase(eager),"The predicate expanded to zero expressions. This may for example be caused by a regex not matching column names or a column dtype match not hitting any dtypes in the DataFrame"+suffix(frame_plan(source)));
  const auto column=index_of(source,attr.value);
  if(!column)fail("ColumnNotFoundError",phase(eager),column_prefix(source,attr.value)+suffix(frame_plan(source)));
  Predicate p{call,*column};p.dtype=source_dtype(source,p.column);
  {
    const auto& query=*call.values;
    if(text(query)&&!index_of(source,query.value))fail("ColumnNotFoundError",phase(eager),column_prefix(source,query.value)+suffix(frame_plan(source)));
    const auto qdtype=list_dtype(query);bool compatible=qdtype=="Null"||p.dtype=="Null";
    if(qdtype=="String")compatible=p.dtype=="String"||p.dtype.starts_with("Categorical")||p.dtype.starts_with("Enum(");
    else if(qdtype=="Int64"||qdtype=="Float64"||qdtype.starts_with("Unknown("))compatible=p.dtype=="Int64"||p.dtype=="UInt64"||p.dtype=="Float64"||p.dtype=="UInt32"||p.dtype=="Null";
    else if(qdtype=="Boolean")compatible=p.dtype=="Boolean"||p.dtype=="Null";
    else if(qdtype=="Date")compatible=p.dtype=="Date"||p.dtype=="Null";
    else if(qdtype.starts_with("Datetime("))compatible=p.dtype.starts_with("Datetime(")||p.dtype=="Null";
    if(!compatible)fail("InvalidOperationError",phase(eager),"'is_in' cannot check for List("+qdtype+") values in "+membership_type(p.dtype)+" data"+suffix(filter_plan(source,membership_expr(attr.value,query,trace||truth(call.options.positive))),true));
    if(collection(query)) {
      if(query.container==AttributeContainerKind::kContainer)for(const auto& v:query.children)p.values.push_back(EventAttribute{"",v.key});
      else p.values=query.children;
    } else if(!null(query))p.values.push_back(query);
  }
  if(trace) {
    const auto& role=call.options.case_id_key;
    if(collection(role))fail("InvalidOperationError",phase(eager),"expected join keys/predicates"+(eager?"":resolved("'join'","UNIQUE[maintain_order: false, keep_strategy: Any] BY None\n  DF []; PROJECT */0 COLUMNS",{"'join'"})));
    const auto plan=filter_plan(source,membership_expr(attr.value,*call.values,true));
    if(integer(role))p.literal_case=true;
    else {
      const auto c=index_of(source,role.value);
      if(!c)fail("ColumnNotFoundError",phase(eager),column_prefix(source,role.value)+suffix(plan));
      p.case_column=c;p.case_dtype=source_dtype(source,*c);
    }
  }
  return p;
}
std::vector<bool> matching(const EventTable& source,const Predicate& p) {
  const auto op=p.call.operation;const bool positive=truth(p.call.options.positive);std::vector<bool> flags(source.size(),false);
  for(std::size_t i=0;i<source.size();++i) {
    const auto v=cell(source,i,p.column);bool yes=false;
    if(numeric(op))yes=!p.empty&&std::all_of(p.chain.begin(),p.chain.end(),[&](const Expr& node){return truth_of(node,source,i)==1;});
    else {
      if(!null(v)) {for(const auto& q:p.values)if(!null(q)&&scalar_equal(v,q)){yes=true;break;}if(!positive&&!traced(op))yes=!yes;}
      else if(p.values.empty()&&collection(*p.call.values)&&!positive&&!traced(op))yes=true;
    }
    flags[i]=yes;
  }
  if(!traced(op))return flags;
  if(p.literal_case){const bool eligible=std::any_of(flags.begin(),flags.end(),[](bool value){return value;});return std::vector<bool>(source.size(),positive?eligible:!eligible);}
  std::unordered_set<std::string> cases;
  for(std::size_t i=0;i<flags.size();++i)if(flags[i]){const auto v=cell(source,i,*p.case_column);if(!null(v))cases.insert(group_key(v,p.case_dtype));}
  for(std::size_t i=0;i<flags.size();++i){const auto v=cell(source,i,*p.case_column);const bool found=!null(v)&&cases.contains(group_key(v,p.case_dtype));flags[i]=positive?found:!found;}
  return flags;
}
EventTable case_projection(EventTable source,const PolarsAttributesFilterCall& call) {
  if(!traced(call.operation)||!integer(call.options.case_id_key))return source;
  if(index_of(source,"literal"))throw std::invalid_argument("Native Polars literal case projection requires no existing literal column");
  const auto n=exact(call.options.case_id_key);std::string dtype;
  EventAttribute value{"literal",n.decimal(),AttributeKind::kInt};
  if(n>=TokenInteger(std::numeric_limits<std::int64_t>::min())&&n<=TokenInteger(std::numeric_limits<std::int64_t>::max())) {
    value.typed_value=static_cast<std::int64_t>(n);
    dtype=n>=TokenInteger(std::numeric_limits<std::int32_t>::min())&&n<=TokenInteger(std::numeric_limits<std::int32_t>::max())?"Int32":"Int64";
  }else{value.typed_value=static_cast<std::uint64_t>(n);dtype="UInt64";}
  source.columns.push_back("literal");source.dtypes.push_back(dtype);
  if(!source.categorical_dtypes.empty())source.categorical_dtypes.push_back(std::nullopt);
  for(auto& row:source.rows)row.push_back(value);
  return source;
}
EventTable filtered(const EventTable& source,const Predicate& query) {
  return case_projection(select_rows(source,matching(source,query)),query.call);
}
struct CountColumn { std::optional<std::size_t> column, cases;EventAttribute literal;std::string dtype;bool once=false; };
CountColumn counts_schema(const EventTable& source,const EventAttribute& attr,const PolarsAttributesFilterOptions& options,bool relative=false) {
  CountColumn result;result.literal=attr;result.once=options.keep_once_per_case&&truth(*options.keep_once_per_case);
  const auto groupnodes=result.once?std::vector<std::string>{"'group_by'","'group_by'"}:std::vector<std::string>{"'group_by'"};
  if(collection(attr)) {
    if(attr.children.empty())fail("IndexError","apply","tuple index out of range");
    // A key list groups by each of its items.
    if(result.once)if(const auto error=detail::polars_list_literal(attr).error)fail("TypeError","apply",*error);
    for(const auto& item:attr.children)if(text(item)&&!index_of(source,item.value))
      if(!result.once)fail("ColumnNotFoundError","apply",column_prefix(source,item.value)+resolved("'group_by'",frame_plan(source),groupnodes));
    throw std::invalid_argument("Native Polars attribute counts accept a single key");
  }
  // Per-case counts group by [case, attribute]: the case key resolves first.
  if(result.once) {
    col_selector(options.case_id_key);
    if(collection(options.case_id_key))fail("IndexError","apply","index 0 is out of bounds for sequence of length 0");
    result.cases=index_of(source,options.case_id_key.value);
    if(!result.cases)fail("ColumnNotFoundError","apply",column_prefix(source,options.case_id_key.value)+resolved("'group_by'",frame_plan(source),groupnodes));
  }
  if(text(attr)) {
    result.column=index_of(source,attr.value);
    if(!result.column)fail("ColumnNotFoundError","apply",column_prefix(source,attr.value)+resolved("'group_by'",frame_plan(source),groupnodes));
    result.dtype=source_dtype(source,*result.column);
  }
  if(result.once) {
    if(result.column==result.cases)fail("DuplicateError","apply","group_by keys contained duplicate output name '"+attr.value+"'. It's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names."+resolved("'group_by'",frame_plan(source),groupnodes));
  }
  return result;
}
PolarsAttributeCounts counts_from(const EventTable& source,const CountColumn& col) {
  PolarsAttributeCounts out;std::unordered_map<std::string,std::size_t> positions;std::unordered_set<std::string> pairs;
  for(std::size_t row=0;row<source.size();++row) {
    auto v=col.column?cell(source,row,*col.column):col.literal;
    const auto key=col.column?group_key(v,col.dtype):"literal";
    if(col.once) {
      const auto k=group_key(cell(source,row,*col.cases),source_dtype(source,*col.cases));
      if(!pairs.insert(std::to_string(k.size())+":"+k+key).second)continue;
    }
    auto [it,inserted]=positions.emplace(key,out.size());if(inserted)out.push_back({clean_value(v),count_value(0)});
    auto& n=out[it->second].count;const auto count=static_cast<std::uint64_t>(exact(n));n=count_value(count+1);
  }
  // Grouping an empty frame by a literal still yields the literal's group.
  if(!col.column&&!col.once&&out.empty())out.push_back({clean_value(col.literal),count_value(0)});
  return out;
}
std::size_t distinct_case_count(const EventTable& source,const EventAttribute& key) {
  col_selector(key);if(collection(key))fail("IndexError","apply","index 0 is out of bounds for sequence of length 0");
  const auto column=index_of(source,key.value);if(!column)fail("ColumnNotFoundError","apply",column_prefix(source,key.value)+resolved("'select'",frame_plan(source),{"'select'"}));
  std::unordered_set<std::string> keys;for(std::size_t i=0;i<source.size();++i)keys.insert(group_key(cell(source,i,*column),source_dtype(source,*column)));return keys.size();
}
PolarsAttributeCounts cache_counts(const PolarsAttributeCountCache& cache) {
  if(auto p=std::get_if<PolarsAttributeCounts>(&cache)) {
    PolarsAttributeCounts normalized;
    for(const auto& entry:*p) {
      auto position=std::find_if(normalized.begin(),normalized.end(),[&](const auto& old){
        if(real(old.value)&&real(entry.value)&&std::isnan(number(old.value))&&std::isnan(number(entry.value)))return false;
        return scalar_equal(old.value,entry.value);
      });
      if(position==normalized.end())normalized.push_back(entry);else position->count=entry.count;
    }
    return normalized;
  }
  const auto& v=std::get<EventAttribute>(cache);if(!collection(v)||v.container!=AttributeContainerKind::kContainer)fail("AttributeError","apply","'"+core_type(v)+"' object has no attribute 'items'");
  PolarsAttributeCounts out;for(const auto& child:v.children)out.push_back({EventAttribute{"",child.key},child});return out;
}
bool python_ge(const EventAttribute& a,const EventAttribute& b) {
  const bool an=integer(a)||boolean(a)||real(a),bn=integer(b)||boolean(b)||real(b);
  if(an&&bn) {
    if(!real(a)&&!real(b))return exact(a)>=exact(b);
    return number(a)>=number(b);
  }
  if(text(a)&&text(b))return a.value>=b.value;
  fail("TypeError","apply","'>=' not supported between instances of '"+core_type(a)+"' and '"+core_type(b)+"'");
}
bool variant_contains(const EventAttribute& variant,const EventAttribute& key) {
  if(collection(variant)) {
    if(variant.container==AttributeContainerKind::kContainer){for(const auto& child:variant.children)if(text(key)&&key.value==child.key)return true;return false;}
    for(const auto& child:variant.children)if(scalar_equal(child,key))return true;return false;
  }
  if(text(variant)) {
    if(!text(key))fail("TypeError","apply","'in <string>' requires string as left operand, not "+core_type(key));
    return variant.value.find(key.value)!=std::string::npos;
  }
  fail("TypeError","apply","argument of type '"+core_type(variant)+"' is not iterable");
}
struct StatisticalSelection { EventAttribute attribute,values; bool unchanged=false; };
StatisticalSelection stats(const EventTable& source,const PolarsAttributesFilterCall& call,bool eager) {
  const auto op=call.operation;const auto& options=call.options;const auto attr=op==Op::kRelative?options.attribute_key:options.activity_key;
  StatisticalSelection selection;selection.attribute=attr;selection.values.container=AttributeContainerKind::kList;
  PolarsAttributeCounts counts;
  const bool explicit_cache=op==Op::kThreshold&&call.count_cache&&!(std::holds_alternative<EventAttribute>(*call.count_cache)&&null(std::get<EventAttribute>(*call.count_cache)));
  CountColumn schema;
  if(!explicit_cache) {
    auto countoptions=options;
    if(op!=Op::kRelative)countoptions.keep_once_per_case.reset();
    if(op==Op::kTopActivities) {
      if(collection(attr)) {if(eager)fail("AttributeError","apply","'DataFrame' object has no attribute 'collect'");fail("AttributeError","apply","'DataFrame' object has no attribute 'to_list'");}
      if(text(attr)&&!index_of(source,attr.value))fail("ColumnNotFoundError","apply",column_prefix(source,attr.value)+resolved(eager?"'group_by'":"'slice'",frame_plan(source),eager?std::vector<std::string>{"'group_by'"}:std::vector<std::string>{"'group_by'","'select'","'slice'"}));
      if(eager)fail("AttributeError","apply","'DataFrame' object has no attribute 'collect'");
      if(null(attr))fail("TypeError","apply","cannot select columns using key of type 'NoneType': None");
      if(integer(attr))fail("IndexError","apply","index "+exact(attr).decimal()+" is out of bounds for DataFrame of height 1");
    }
    schema=counts_schema(source,attr,countoptions,op==Op::kRelative);
    if(eager)fail("AttributeError","apply","'DataFrame' object has no attribute 'collect'");
    counts=counts_from(source,schema);
  } else counts=cache_counts(*call.count_cache);
  if(op==Op::kTopActivities) {
    const auto maximum=call.maximum.value_or(EventAttribute{"","25",AttributeKind::kInt});
    if(!null(maximum)) {
      if(text(maximum))fail("TypeError","apply","'<' not supported between instances of 'str' and 'int'");
      if(!integer(maximum)&&!boolean(maximum))fail("TypeError","apply","'"+core_type(maximum)+"' object cannot be interpreted as an integer");
      const auto n=exact(maximum);
      if(n<0)fail("ValueError","apply","negative slice lengths ("+n.decimal()+") are invalid for LazyFrame");
      if(n>TokenInteger(std::numeric_limits<std::int64_t>::max()))fail("OverflowError","apply","number too large to fit in target type");
      std::stable_sort(counts.begin(),counts.end(),[](const auto& a,const auto& b){return exact(a.count)>exact(b.count);});
      if(n<TokenInteger(counts.size()))counts.resize(static_cast<std::size_t>(n));
    }
    for(const auto& c:counts)selection.values.children.push_back(c.value);
    // Public helper bypasses the predicate if all source activities survive.
    const auto original=counts_from(source,schema);selection.unchanged=counts.size()==original.size();return selection;
  }
  EventAttribute threshold;
  if(op==Op::kRelative) {
    const auto& stake=*call.stake;std::size_t denominator=0;
    const bool case_denominator=!options.keep_once_per_case||truth(*options.keep_once_per_case);
    denominator=case_denominator?distinct_case_count(source,options.case_id_key):source.size();
    // The stake multiplies the denominator once per counted value.
    if(counts.empty())return selection;
    if(null(stake)||date(stake))fail("TypeError","apply","unsupported operand type(s) for *: '"+core_type(stake)+"' and 'int'");
    if(text(stake)||collection(stake))threshold=stake;
    else {threshold=EventAttribute{"",real_text(number(stake)*static_cast<double>(denominator)),AttributeKind::kFloat};threshold.typed_value=number(stake)*static_cast<double>(denominator);}
  } else threshold=*call.threshold;
  for(const auto& item:counts) {
    const bool admitted=python_ge(item.count,threshold);
    const bool variant=!admitted&&call.most_common_variant&&!null(*call.most_common_variant)&&variant_contains(*call.most_common_variant,item.value);
    if(admitted||variant)selection.values.children.push_back(item.value);
  }
  selection.unchanged=op==Op::kThreshold&&selection.values.children.size()==counts.size();return selection;
}
EventTable stats_output(const EventTable& source,const PolarsAttributesFilterCall& call,const StatisticalSelection& selection,bool eager) {
  if(selection.unchanged)return source;
  if(call.operation==Op::kRelative)col_selector(selection.attribute);
  auto filtering=call;filtering.operation=Op::kEvents;filtering.values=selection.values;filtering.options.attribute_key=selection.attribute;
  if(call.operation!=Op::kRelative)filtering.options.positive=EventAttribute{"","true",AttributeKind::kBoolean};
  return select_rows(source,matching(source,predicate(source,filtering,eager)));
}
EventTable metadata_table(const LazyEventTable& source) {
  EventTable out;out.columns=source.columns();out.dtypes=source.dtypes();out.categorical_dtypes=source.categorical_dtypes();out.attributes=source.attributes();out.metadata=source.metadata();return out;
}
LazyEventTable deferred(const LazyEventTable& source,std::function<EventTable(const EventTable&)> transform,const std::optional<EventTable>& output={}) {
  return LazyEventTable(output?output->columns:source.columns(),[source,transform=std::move(transform)](const auto& consume){const auto table=transform(source.materialize());for(const auto& row:table.rows)consume(row);},source.attributes(),source.metadata(),std::nullopt,output?output->dtypes:source.dtypes(),std::nullopt,output?output->categorical_dtypes:source.categorical_dtypes());
}
}  // namespace

PolarsAttributesFilterResult invoke_polars_attributes_filter(const EventTable& source,const PolarsAttributesFilterCall& input) {
  source.validate();const auto call=prepare(input);
  if(call.operation==Op::kCounts){const auto schema=counts_schema(source,*call.attribute,call.options);(void)schema;fail("AttributeError","apply","'DataFrame' object has no attribute 'collect'");}
  if(statistics(call.operation)){const auto selected=stats(source,call,true);return stats_output(source,call,selected,true);}
  const auto query=predicate(source,call,true);return filtered(source,query);
}
LazyPolarsAttributesFilterResult invoke_polars_attributes_filter(const LazyEventTable& source,const PolarsAttributesFilterCall& input) {
  const auto call=prepare(input);const auto meta=metadata_table(source);meta.validate();
  if(call.operation==Op::kCounts) {
    const auto schema=counts_schema(meta,*call.attribute,call.options);
    return counts_from(source.materialize(),schema);
  }
  if(statistics(call.operation)) {
    // Statistics is an actual apply-time scan, followed by a restartable lazy
    // predicate over the original source. Diagnostics keep the source kind.
    const auto materialized=source.materialize();const auto selected=stats(materialized,call,false);
    return deferred(source,[call,selected](const auto& table){return stats_output(table,call,selected,false);});
  }
  return deferred(source,[call](const auto& table){const auto query=predicate(table,call,false);return filtered(table,query);},case_projection(meta,call));
}

EventTable filter_polars_attributes(const EventTable& source, const EventAttribute& value, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kTraces;call.options=options;call.values=value;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes(const LazyEventTable& source, const EventAttribute& value, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kTraces;call.options=options;call.values=value;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

EventTable filter_polars_attributes_events(const EventTable& source, const EventAttribute& value, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kEvents;call.options=options;call.values=value;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes_events(const LazyEventTable& source, const EventAttribute& value, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kEvents;call.options=options;call.values=value;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

EventTable filter_polars_attributes_numeric(const EventTable& source, const EventAttribute& lower, const EventAttribute& upper, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kNumericTraces;call.options=options;call.lower=lower;call.upper=upper;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes_numeric(const LazyEventTable& source, const EventAttribute& lower, const EventAttribute& upper, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kNumericTraces;call.options=options;call.lower=lower;call.upper=upper;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

EventTable filter_polars_attributes_numeric_events(const EventTable& source, const EventAttribute& lower, const EventAttribute& upper, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kNumericEvents;call.options=options;call.lower=lower;call.upper=upper;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes_numeric_events(const LazyEventTable& source, const EventAttribute& lower, const EventAttribute& upper, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kNumericEvents;call.options=options;call.lower=lower;call.upper=upper;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

EventTable filter_polars_attributes_on_values(const EventTable& source, const EventAttribute& value, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kOnValues;call.options=options;call.values=value;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes_on_values(const LazyEventTable& source, const EventAttribute& value, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kOnValues;call.options=options;call.values=value;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

EventTable filter_polars_attributes_relative(const EventTable& source, const EventAttribute& stake, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kRelative;call.options=options;call.stake=stake;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes_relative(const LazyEventTable& source, const EventAttribute& stake, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kRelative;call.options=options;call.stake=stake;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

EventTable filter_polars_attributes_top_activities(const EventTable& source, const EventAttribute& maximum, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kTopActivities;call.options=options;call.maximum=maximum;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes_top_activities(const LazyEventTable& source, const EventAttribute& maximum, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kTopActivities;call.options=options;call.maximum=maximum;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

EventTable filter_polars_attributes_threshold(const EventTable& source, const EventAttribute& threshold, const PolarsAttributesFilterOptions& options, const std::optional<PolarsAttributeCountCache>& cache, const std::optional<EventAttribute>& variant) {
  PolarsAttributesFilterCall call;call.operation=Op::kThreshold;call.options=options;call.threshold=threshold;call.count_cache=cache;call.most_common_variant=variant;
  return std::get<EventTable>(invoke_polars_attributes_filter(source,call));
}

LazyEventTable filter_polars_attributes_threshold(const LazyEventTable& source, const EventAttribute& threshold, const PolarsAttributesFilterOptions& options, const std::optional<PolarsAttributeCountCache>& cache, const std::optional<EventAttribute>& variant) {
  PolarsAttributesFilterCall call;call.operation=Op::kThreshold;call.options=options;call.threshold=threshold;call.count_cache=cache;call.most_common_variant=variant;
  return std::get<LazyEventTable>(invoke_polars_attributes_filter(source,call));
}

PolarsAttributeCounts get_polars_attribute_values(const EventTable& source, const EventAttribute& attribute, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kCounts;call.options=options;call.attribute=attribute;
  return std::get<PolarsAttributeCounts>(invoke_polars_attributes_filter(source,call));
}

PolarsAttributeCounts get_polars_attribute_values(const LazyEventTable& source, const EventAttribute& attribute, const PolarsAttributesFilterOptions& options) {
  PolarsAttributesFilterCall call;call.operation=Op::kCounts;call.options=options;call.attribute=attribute;
  return std::get<PolarsAttributeCounts>(invoke_polars_attributes_filter(source,call));
}
}  // namespace minera
