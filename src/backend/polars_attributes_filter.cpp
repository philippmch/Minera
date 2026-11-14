#include "minera/polars_attributes_filter.hpp"
#include "polars_datetime.hpp"
#include "polars_column_hint.hpp"
#include "polars_float_text.hpp"
#include "polars_list_literal.hpp"
#include "polars_lit_model.hpp"
#include "pandas_parameter_carrier.hpp"

#include <sstream>
#include <chrono>
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
    // Round CE: a list or tuple as pl.lit built it.
    if(value.container!=AttributeContainerKind::kContainer)
      try{return detail::polars_lit::literal_list_display(detail::polars_lit::argument_literal(value));}catch(const PolarsSequenceFilterError&){}
    // Polars prints the first two and the last item of a longer list.
    const auto& items=value.children;std::string out="[";
    for(std::size_t i=0;i<items.size();++i) {
      if(items.size()>3&&i==2){out+=", \xe2\x80\xa6 "+expression_literal(items.back());break;}
      if(i)out+=", ";out+=expression_literal(items[i]);
    }
    return out+"]";
  }
  if(boolean(value))return truth(value)?"true":"false";
  if(date(value))return detail::polars_lit::scalar_display(detail::polars_lit::value_of(value));
  if(integer(value))return decimal?real_text(number(value))+ (real_text(number(value)).find('.')==std::string::npos?".0":"") : (dynamic?"dyn int: ":"")+exact(value).decimal();
  // Round CF: a dynamic float prints as Rust's Display (1e17 in full).
  if(real(value)&&dynamic)return "dyn float: "+detail::polars_dyn_float(number(value));
  if(real(value)) {auto out=real_text(number(value));if(decimal && std::isfinite(number(value)) && out.find_first_of(".eE")==std::string::npos)out+=".0";return out;}
  return quote(value.value);
}
std::string membership_expr(const std::string& key,const EventAttribute& values,bool positive) {
  std::string operand;
  // Polars folds membership in an empty literal list to a false literal.
  if(positive&&collection(values)&&values.container!=AttributeContainerKind::kContainer&&values.children.empty())return "false.alias("+quote(key)+")";
  // Round CF: a None scalar folds the membership to a false literal too.
  if(positive&&null(values))return "false.alias("+quote(key)+")";
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
    auto literal=[](const EventAttribute& v){
      if(!collection(v)||v.container==AttributeContainerKind::kContainer)return;
      try{(void)detail::polars_lit::literal_of(detail::polars_lit::value_of(v));}
      catch(const PolarsSequenceFilterError& error){fail(error.type,"apply",error.message);}
    };
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
struct Leaf { std::string column, op, dtype; EventAttribute value; std::vector<std::string> categories; };
struct Expr { std::optional<Leaf> leaf; std::string op; std::vector<Expr> kids; };
struct Predicate {
  PolarsAttributesFilterCall call;
  std::size_t column=0;
  std::optional<std::size_t> case_column, stream1, stream2;
  std::string dtype, case_dtype;
  bool literal_case=false;
  std::vector<EventAttribute> values;
  // Round CD: a list (tuple, dict keys) argument as pl.lit builds it.
  std::optional<detail::polars_lit::Literal> literal;
  std::vector<Expr> chain,merged;
  bool empty=false;
  // Round CH: a str value naming a column (is_in(col(value).implode())).
  std::optional<std::size_t> other;
  std::string other_dtype,other_super,other_plan;
  // Round CH: an error the plan raises while collecting (a String column
  // strictly cast to the other column's Enum).
  std::string collect_error;
};
// Round CH: a str value that names a column of the frame is pl.col(value),
// and is_in reads that column imploded into one list (probed on 19 x 19
// column dtypes against Polars 1.44.1). Numbers meet at their supertype (a
// Float64 side wins; Float32 with ints of up to 16 bits stays Float32, else
// Float64; ints of one signedness the wider; a signed and an unsigned int
// the signed int of twice the unsigned width, Int128 with UInt64), an int
// side cast to a float with strict_cast, others with cast. A Null side casts
// to the other's type; String meets Categorical or Enum by a strict cast to
// it; a Datetime column reads another Datetime column's physical values
// unless the other is finer (an error). Any other pair raises "'is_in'
// cannot check for List(Z) values in X data".
int integer_bits(const std::string& d){return std::stoi(d.substr(d.rfind("UInt",0)==0?4:3));}
std::string numeric_supertype(const std::string& a,const std::string& b) {
  if(a==b)return a;
  if(a=="Float64"||b=="Float64")return "Float64";
  if(a=="Float32"||b=="Float32")return integer_bits(a=="Float32"?b:a)<=16?"Float32":"Float64";
  const bool ua=a.rfind("UInt",0)==0,ub=b.rfind("UInt",0)==0;const int ba=integer_bits(a),bb=integer_bits(b);
  if(ua==ub)return ba>=bb?a:b;
  const int sb=ua?bb:ba,ubits=ua?ba:bb;
  if(ubits==64)return "Int128";
  return "Int"+std::to_string(std::max(sb,2*ubits));
}
std::string unit_word(std::int64_t unit){return unit==1?"Nanoseconds":unit==1000?"Microseconds":"Milliseconds";}
struct ColumnIn{std::string error,lhs,rhs,super;};
ColumnIn column_is_in(const std::string& key,const std::string& x,const std::string& name,const std::string& z);
bool numeric_column(const std::string& dtype);
// An Enum column's categories in order.
std::vector<std::string> enum_category_list(const EventTable& table,std::size_t column) {
  std::vector<std::string> out;
  if(column<table.categorical_dtypes.size()&&table.categorical_dtypes[column]){for(const auto& c:table.categorical_dtypes[column]->categories)out.push_back(c.value);return out;}
  const auto& dtype=table.dtypes[column];
  for(auto at=dtype.find('\'');at!=std::string::npos;) {
    const auto end=dtype.find('\'',at+1);if(end==std::string::npos)break;
    out.push_back(dtype.substr(at+1,end-at-1));at=dtype.find('\'',end+1);
  }
  return out;
}
// col(key).is_in(literal) as a plan prints it: before resolution with the
// literal as built, once resolved with its coercion to the column (round CD);
// a positive membership in an empty list folds to a false literal.
std::string list_membership(const std::string& key,const std::string& dtype,const detail::polars_lit::Literal& literal,bool positive,bool coerced,bool empty_argument) {
  if(positive&&empty_argument)return "false.alias("+quote(key)+")";
  const auto text=coerced?detail::polars_lit::coerced_is_in(key,dtype,literal):"col("+quote(key)+").is_in("+detail::polars_lit::literal_text(literal)+")";
  return text+(positive?"":".not()");
}
bool numeric_column(const std::string& dtype) {
  return detail::polars_lit::integer_dtype(dtype)||dtype=="Float64"||dtype=="Float32";
}
ColumnIn column_is_in(const std::string& key,const std::string& x,const std::string& name,const std::string& z) {
  namespace lit=detail::polars_lit;
  ColumnIn out{{},"col("+quote(key)+")","col("+quote(name)+").implode()",x};
  if(x==z)return out;
  if(x=="Null"){out.lhs+=".cast("+lit::is_in_dtype_name(z)+")";out.super=z;return out;}
  if(z=="Null"){out.rhs+=".cast(List("+lit::is_in_dtype_name(x)+"))";return out;}
  if(numeric_column(x)&&numeric_column(z)) {
    const auto s=numeric_supertype(x,z);out.super=s;
    const auto cast=[&](const std::string& from){return std::string(lit::float_dtype(s)&&!lit::float_dtype(from)?".strict_cast(":".cast(");};
    if(x!=s)out.lhs+=cast(x)+s+")";
    if(z!=s)out.rhs+=cast(z)+"List("+s+"))";
    return out;
  }
  const bool cx=lit::categorical_dtype(x)||lit::enum_dtype(x),cz=lit::categorical_dtype(z)||lit::enum_dtype(z);
  if(x=="String"&&cz){out.lhs+=".strict_cast("+lit::is_in_dtype_name(z)+")";out.super=z;return out;}
  if(cx&&z=="String"){out.rhs+=".strict_cast(List("+lit::is_in_dtype_name(x)+"))";return out;}
  if(lit::datetime_dtype(x)&&lit::datetime_dtype(z)) {
    const auto ux=lit::unit_nanoseconds(x),uz=lit::unit_nanoseconds(z);
    if(uz<ux)out.error="'is_in' cannot check for "+unit_word(uz)+" precision values in "+unit_word(ux)+" Datetime data";
    return out;
  }
  out.error="'is_in' cannot check for List("+lit::is_in_dtype_name(z)+") values in "+lit::is_in_dtype_name(x)+" data";
  return out;
}
// A present cell of x is in the other column (present cells of z).
bool column_has(const EventAttribute& v,const std::string& x,const std::vector<EventAttribute>& others,const std::string& z,const std::string& s) {
  namespace lit=detail::polars_lit;
  if(x=="Null"||z=="Null")return false;
  for(const auto& w:others) {
    if(numeric_column(x)&&numeric_column(z)) {
      if(lit::integer_dtype(s)){if(exact(v)==exact(w))return true;continue;}
      const double a=s=="Float32"?static_cast<double>(static_cast<float>(number(v))):number(v);
      const double b=s=="Float32"?static_cast<double>(static_cast<float>(number(w))):number(w);
      if(a==b||(std::isnan(a)&&std::isnan(b)))return true;
      continue;
    }
    if(x=="Date"||lit::datetime_dtype(x)) {
      const auto a=lit::physical_of(v,x),b=lit::physical_of(w,z);
      if(a&&b&&*a==*b)return true;
      continue;
    }
    if(scalar_equal(v,w))return true;
  }
  return false;
}
std::string list_dtype(const EventAttribute& query) {
  if(!collection(query)) {
    if(boolean(query))return "Boolean";
    if(integer(query))return "Unknown(Int("+exact(query).decimal()+"))";
    if(real(query))return "Unknown(Float)";
    if(null(query))return "Null";
    if(date(query))return detail::polars_lit::debug_name(detail::polars_lit::lit_value(detail::polars_lit::value_of(query)).type);
    return "String";
  }
  if(query.container==AttributeContainerKind::kContainer)return "String";
  // Round CE: the element type as pl.lit builds the list.
  try{return detail::polars_lit::debug_name(detail::polars_lit::argument_literal(query).type);}catch(const PolarsSequenceFilterError&){}
  for(const auto& v:query.children)if(!null(v)) {
    if(collection(v))return "List("+list_dtype(v)+")";
    if(date(v))return detail::polars_lit::debug_name(detail::polars_lit::lit_value(detail::polars_lit::value_of(v)).type);
    return detail::polars_literal_dtype(v);
  }
  return "Null";
}
std::string membership_type(const std::string& dtype) { return detail::polars_lit::is_in_dtype_name(dtype); }
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
bool integer_dtype(const std::string& dtype) { return dtype=="Int64"||dtype=="UInt64"; }
bool numeric_dtype(const std::string& dtype) { return integer_dtype(dtype)||dtype=="Float64"; }
// Columns Polars compares with a numeric literal through a physical integer:
// Boolean as 0/1 (cast to Int32), Date as days and Datetime as ticks of its
// unit since the epoch (round BW), Null as null.
bool physical_dtype(const std::string& dtype) {
  return dtype=="Boolean"||dtype=="Date"||dtype=="Null"||polars_datetime::datetime(dtype);
}
std::int64_t datetime_ticks(const EventAttribute& v,const std::string& dtype) {
  const auto parsed=polars_datetime::ticks(date_text(v),polars_datetime::column_unit(dtype));
  if(!parsed)throw std::invalid_argument("Native Polars numeric filter requires valid declared temporal cells");return *parsed;
}
bool plain_number(const EventAttribute& v) { return (integer(v)&&!boolean(v)&&!date(v))||real(v); }
// Days since 1970-01-01 of a Polars Date cell ("YYYY-MM-DD...").
std::int64_t epoch_days(const EventAttribute& v) {
  const auto text=date_text(v);
  int y=0;unsigned m=0,d=0;char a=0,b=0;std::istringstream in(text.substr(0,10));
  if(!(in>>y>>a>>m>>b>>d)||a!='-'||b!='-')throw std::invalid_argument("Native Polars Date cell is not a date");
  return std::chrono::sys_days{std::chrono::year{y}/std::chrono::month{m}/std::chrono::day{d}}.time_since_epoch().count();
}
bool null_leaf(const Leaf& l) { return null(l.value); }
// Round CD: an integer literal above the Int64 range, which Polars types
// UInt64 (the literal model holds signed 64-bit integers, so these take
// their own path): a UInt64 column compares it exactly, a Boolean one cast
// to UInt64; float columns and signed integer columns (cast to Float64)
// compare it as a float; a narrower unsigned column folds it to a
// constant; temporal columns keep it as a dyn int that matches nothing;
// String and categorical columns refuse it (u64).
std::optional<std::uint64_t> wide_literal(const EventAttribute& v) {
  constexpr auto top=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if(const auto* u=std::get_if<std::uint64_t>(&v.typed_value))return *u>top?std::optional<std::uint64_t>(*u):std::nullopt;
  if(collection(v)||!integer(v)||!std::holds_alternative<std::monostate>(v.typed_value))return {};
  std::uint64_t u=0;const auto r=std::from_chars(v.value.data(),v.value.data()+v.value.size(),u);
  if(r.ec==std::errc{}&&r.ptr==v.value.data()+v.value.size()&&u>top)return u;
  return {};
}
std::optional<detail::polars_lit::Operands> wide_operands(const Leaf& l) {
  namespace lit=detail::polars_lit;
  const auto u=wide_literal(l.value);if(!u)return {};
  const auto c="col("+quote(l.column)+")";const auto decimal=std::to_string(*u);const double d=static_cast<double>(*u);
  if(l.dtype=="UInt64")return lit::Operands{c,decimal,{}};
  if(l.dtype=="Boolean")return lit::Operands{c+".cast(UInt64)",decimal,{}};
  // Round CH: an int rounds to Float32 straight from its digits.
  if(l.dtype=="Float32")return lit::Operands{c,lit::polars_frame_float(lit::float32_of_u64(*u)),{}};
  if(l.dtype=="Float64")return lit::Operands{c,lit::polars_frame_float(d),{}};
  if(l.dtype.starts_with("UInt"))return lit::Operands{{},{},"when("+c+".is_not_null()).then("+(lit::apply_order(-1,lit::compare_op(l.op))?"true":"false")+").otherwise(null)"};
  if(lit::integer_dtype(l.dtype))return lit::Operands{c+".cast(Float64)",lit::polars_frame_float(d),{}};
  return lit::Operands{c,"dyn int: "+decimal,{}};
}
int wide_truth(const Leaf& l,const EventAttribute& v,std::uint64_t u) {
  namespace lit=detail::polars_lit;
  const auto op=lit::compare_op(l.op);const double d=static_cast<double>(u);
  if(l.dtype=="UInt64")return lit::apply_order(lit::order_of(lit::wide_of(v),lit::Wide(u)),op);
  if(l.dtype=="Float32")return lit::apply_order(lit::total_order(static_cast<double>(static_cast<float>(number(v))),lit::float32_of_u64(u)),op);
  if(l.dtype=="Float64"||(lit::integer_dtype(l.dtype)&&!l.dtype.starts_with("UInt")))return lit::apply_order(lit::total_order(number(v),d),op);
  if(l.dtype=="Boolean"||lit::integer_dtype(l.dtype))return lit::apply_order(-1,op);
  return -1;
}
// The (lhs, rhs) operands Polars prints once the literal is coerced to the
// column; a None literal folds the comparison to a null column.
// Round CD: scalars as detail::polars_lit types them for any column dtype
// (uncoerced when the comparison fails); lists as built.
detail::polars_lit::Operands operands(const Leaf& l,bool cast_list=false) {
  namespace lit=detail::polars_lit;
  const auto column="col("+quote(l.column)+")";
  const auto& v=l.value;
  if(collection(v)) {
    if(l.dtype=="String"||l.dtype.starts_with("Categorical")||l.dtype.starts_with("Enum("))return {column,expression_literal(v)+(cast_list?".cast(String)":""),{}};
    // Round CE: a Null column casts to the list's type.
    if(l.dtype=="Null"&&v.container!=AttributeContainerKind::kContainer)
      try{return {column+".cast("+lit::debug_name(lit::list_of(lit::argument_literal(v).type))+")",expression_literal(v),{}};}catch(const PolarsSequenceFilterError&){}
    return {column,expression_literal(v),{}};
  }
  if(auto wide=wide_operands(l))return *wide;
  const auto value=lit::value_of(v);const auto op=lit::compare_op(l.op);
  if(!lit::check_scalar(l.column,l.dtype,value,op,l.categories.empty()?nullptr:&l.categories).ok())return {column,lit::scalar_display(value),{}};
  return lit::coerced_operands(l.column,l.dtype,value,op);
}
std::string null_text(const Leaf& l) { return "null.repeat([col("+quote(l.column)+").len()])"; }
// cast: the leaf whose resolution failed. Round CE: its node prints the
// comparisons before it coerced, the ones after it as written.
std::string plan_text_at(const Expr& e,const Leaf* cast,bool& past) {
  if(e.leaf) {
    const auto& l=*e.leaf;
    if(&l==cast){past=true;return operands(l,true).text(detail::polars_lit::compare_op(l.op));}
    if(past)return "col("+quote(l.column)+") "+l.op+" "+(null_leaf(l)?std::string("null"):collection(l.value)?expression_literal(l.value):detail::polars_lit::scalar_display(detail::polars_lit::value_of(l.value)));
    if(null_leaf(l))return null_text(l);
    return operands(l).text(detail::polars_lit::compare_op(l.op));
  }
  auto wrap=[&](const Expr& k){const bool folded=k.leaf&&null_leaf(*k.leaf)&&!past&&&*k.leaf!=cast;const auto t=plan_text_at(k,cast,past);return folded?t:"("+t+")";};
  const auto left=wrap(e.kids[0]);
  return left+" "+e.op+" "+wrap(e.kids[1]);
}
std::string plan_text(const Expr& e,const Leaf* cast=nullptr) { bool past=false;return plan_text_at(e,cast,past); }
// Round CD: an evaluated folded form prints bare, a when-then with a leading
// dot (as filter_traces_attribute_in_timeframe prints it).
std::string error_text(const Expr& e) {
  if(e.leaf){if(null_leaf(*e.leaf))return null_text(*e.leaf);const auto o=operands(*e.leaf);return o.whole.empty()?"[("+o.lhs+") "+e.leaf->op+" ("+o.rhs+")]":o.whole.starts_with("when(")?"."+o.whole:o.whole;}
  return "[("+error_text(e.kids[0])+") "+e.op+" ("+error_text(e.kids[1])+")]";
}
std::string chain_plan(const EventTable& table,const std::vector<std::string>& filters) {
  std::string out,indent;
  for(auto p=filters.rbegin();p!=filters.rend();++p){out+=indent+"FILTER "+*p+"\n"+indent+"FROM\n";indent+="  ";}
  return out+indent+frame_plan(table);
}
const Expr* leaf_node(const Expr& e) { return e.leaf?&e:nullptr; }
// Predicate pushdown (round CE, modelled on 12,000 random chains in Polars
// 1.44.1): walking down, a filter joins the pending predicates, keyed by its
// column; a predicate equal to one already under its key is dropped, else it
// is combined in front of them (lower filters to the left). A filter on a
// null column (null.repeat) stays in place: the pending predicates join it.
// A node folds its keys left to right in bytewise order, the null filter
// right after the predicates of its own column.
std::string predicate_column(const Expr& e) { std::string out;each_leaf(e,[&](const Leaf& l){if(out.empty())out=l.column;});return out; }
void conjuncts(const Expr& e,std::vector<const Expr*>& out) {
  if(!e.leaf&&e.op=="&"){conjuncts(e.kids[0],out);conjuncts(e.kids[1],out);}else out.push_back(&e);
}
Expr fold_left(std::vector<Expr> items) {
  Expr acc=std::move(items.front());
  for(std::size_t i=1;i<items.size();++i)acc=binary(std::move(acc),"&",std::move(items[i]));
  return acc;
}
// Round CD: Polars merges the comparisons of one operand with typed
// literals into an interval, in the order the filters apply: a tighter bound
// replaces the current one, an equality bounds both sides, and a literal
// without an order against the current bound (another type, NaN) leaves it.
// The merge replaces the comparisons only when it leaves fewer of them: an
// empty interval empties the frame, a point becomes ==, else the bounds.
// hi: the upper key of an == on a nanosecond column, which Polars prints as
// is_between([v, v + 999 ns]) and merges as both bounds (round CE).
struct MergeKey { int tag=0; detail::polars_lit::Wide i; double d=0; std::string s; std::optional<detail::polars_lit::Wide> hi; };
std::optional<MergeKey> merge_key(const Leaf& l) {
  namespace lit=detail::polars_lit;
  if(null(l.value))return {};
  if(l.op!=">="&&l.op!="<="&&l.op!=">"&&l.op!="<"&&l.op!="==")return {};
  const auto o=operands(l);
  // Round CE: only comparisons on the bare column merge (a cast operand
  // never does; probed on Boolean, integer and temporal columns).
  const bool ns_point=l.op=="=="&&o.whole.starts_with("col("+quote(l.column)+").is_between([");
  if(!ns_point&&o.lhs!="col("+quote(l.column)+")")return {};
  // Round CF: list literals only compare equal or not at all (probed: [1]
  // and [3] never merge or fold, [1] and [1.0] neither).
  if(collection(l.value)) {
    MergeKey k;k.tag=20;
    try{k.s=lit::literal_list_key(lit::argument_literal(l.value));}catch(const PolarsSequenceFilterError&){return {};}
    return k;
  }
  if((!o.whole.empty()&&!ns_point)||o.rhs.rfind("dyn ",0)==0)return {};
  if(const auto u=wide_literal(l.value)) {
    MergeKey k;
    if(l.dtype=="UInt64"||l.dtype=="Boolean"){k.i=lit::Wide(*u);return k;}
    if(!lit::float_dtype(l.dtype)&&!lit::integer_dtype(l.dtype))return {};
    k.tag=1;k.d=l.dtype=="Float32"?lit::float32_of_u64(*u):static_cast<double>(*u);return k;
  }
  const auto v=lit::value_of(l.value);const auto op=lit::compare_op(l.op);
  const auto check=lit::check_scalar(l.column,l.dtype,v,op,l.categories.empty()?nullptr:&l.categories);
  MergeKey k;
  // Round CE: str literals on an Enum column merge as text, members or not.
  if(lit::enum_dtype(l.dtype)&&v.kind==lit::Kind::kString){k.tag=2;k.s=v.text;return k;}
  if(!check.ok()) {
    // A typed literal the comparison cannot coerce keeps its own type.
    switch(v.kind){
      case lit::Kind::kString:k.tag=10;k.s=v.text;break;
      case lit::Kind::kDate:k.tag=11;k.i=v.integer;break;
      case lit::Kind::kDatetime:k.tag=12;k.i=lit::literal_micros(v);k.s=lit::lit_value(v).type.zone;break;
      case lit::Kind::kBool:k.tag=13;k.i=v.boolean;break;
      default:return {};
    }
    return k;
  }
  const auto& d=l.dtype;
  if(d=="String"||d.starts_with("Categorical")||d.starts_with("Enum(")){k.tag=2;k.s=v.kind==lit::Kind::kBool?(v.boolean?"true":"false"):v.text;return k;}
  if(d=="Boolean"){if(v.kind==lit::Kind::kString){k.tag=2;k.s=v.text;}else{k.tag=0;k.i=v.kind==lit::Kind::kBool?v.boolean:v.integer;}return k;}
  const auto micros=[&]{return lit::Wide(lit::literal_micros(v));};
  if(lit::integer_dtype(d)) {
    switch(v.kind){
      case lit::Kind::kFloat:k.tag=1;k.d=v.real;break;
      case lit::Kind::kBool:k.i=v.boolean;break;
      case lit::Kind::kDatetime:k.i=micros();break;
      default:k.i=v.integer;break;
    }
    return k;
  }
  if(lit::float_dtype(d)) {
    const bool f32=d=="Float32";const auto narrow=[&](double y){return f32?static_cast<double>(static_cast<float>(y)):y;};
    k.tag=1;
    switch(v.kind){
      case lit::Kind::kFloat:k.d=narrow(v.real);break;
      case lit::Kind::kInt:k.d=f32?lit::float32_of_int(v):static_cast<double>(v.integer);break;
      case lit::Kind::kBool:k.d=v.boolean?1.0:0.0;break;
      case lit::Kind::kDate:k.d=static_cast<double>(v.integer);break;
      default:k.d=static_cast<double>(lit::literal_micros(v));break;
    }
    return k;
  }
  if(d=="Date"){k.i=v.kind==lit::Kind::kDate?lit::Wide(v.integer):micros();return k;}
  if(d.starts_with("Datetime")) {
    const auto unit=lit::unit_nanoseconds(d);
    if(v.kind==lit::Kind::kDate){k.i=lit::wide_mul(v.integer,86'400'000'000'000LL/unit);return k;}
    const auto m=lit::literal_micros(v);
    k.i=unit==1?lit::wide_add(lit::wide_mul(m,1000),op==lit::CompareOp::kLe?999:0):lit::Wide(unit==1000?m:m/1000);
    if(ns_point&&unit==1)k.hi=lit::wide_add(lit::wide_mul(m,1000),999);
    if(ns_point&&!k.hi)return {};
    return k;
  }
  return {};
}
std::optional<int> merge_order(const MergeKey& a,const MergeKey& b) {
  if(a.tag!=b.tag)return {};
  // Round CF: floats in their total order, NaN above every number and
  // equal to itself (probed: >= NaN with <= -1000.0 empties the frame,
  // >= NaN with <= NaN merges into == NaN).
  if(a.tag==1) {
    const bool x=std::isnan(a.d),y=std::isnan(b.d);
    if(x||y)return x&&y?0:x?1:-1;
    return a.d<b.d?-1:a.d>b.d?1:0;
  }
  if(a.tag==20){if(a.s==b.s)return 0;return {};}
  if(a.tag==2||a.tag==10)return a.s<b.s?-1:a.s>b.s?1:0;
  if(a.tag==12&&a.s!=b.s)return {};
  return a.i<b.i?-1:a.i>b.i?1:0;
}
struct Bound { MergeKey key; bool inclusive=true; Leaf leaf; };
// Merges one operand's comparisons (application order) into its bounds; an
// empty vector: an empty interval.
std::vector<Leaf> merge_interval(const std::vector<std::pair<Leaf,MergeKey>>& items) {
  std::optional<Bound> lower,upper;
  const auto tighten=[](std::optional<Bound>& bound,const Bound& next,int direction) {
    if(!bound){bound=next;return;}
    const auto c=merge_order(next.key,bound->key);
    if(!c)return;
    if(*c*direction>0||(*c==0&&!next.inclusive&&bound->inclusive))bound=next;
  };
  for(const auto& [leaf,key]:items) {
    const bool strict=leaf.op==">"||leaf.op=="<";
    if(leaf.op==">="||leaf.op==">"||leaf.op=="==")tighten(lower,Bound{key,!strict,leaf},1);
    if(leaf.op=="<="||leaf.op=="<"||leaf.op=="=="){auto up=key;if(key.hi)up.i=*key.hi;tighten(upper,Bound{up,!strict,leaf},-1);}
  }
  std::vector<Leaf> out;
  const auto bound_leaf=[](const Bound& b,bool low){auto l=b.leaf;l.op=low?(b.inclusive?">=":">"):(b.inclusive?"<=":"<");return l;};
  if(lower&&upper) {
    const auto c=merge_order(lower->key,upper->key);
    if(c&&(*c>0||(*c==0&&!(lower->inclusive&&upper->inclusive))))return out;
    if(c&&*c==0){auto l=lower->leaf;l.op="==";out.push_back(l);}
    else{out.push_back(bound_leaf(*lower,true));out.push_back(bound_leaf(*upper,false));}
  } else out.push_back(bound_leaf(lower?*lower:*upper,lower.has_value()));
  return out;
}
// Round CE: the interval merge runs on a node's whole conjunction. It changes
// the node only when it leaves fewer comparisons in all: then the node is
// rebuilt as the comparisons it did not merge, in order, followed by each
// operand's bounds (lower, upper; operands in order of first appearance),
// folded left. An empty interval empties the frame below the lowest filter
// (scan = true); in a null filter's node it leaves the node unchanged.
Expr merge_node(Expr node,bool scan,bool& empty) {
  std::vector<const Expr*> leaves;conjuncts(node,leaves);
  std::vector<std::string> order;std::map<std::string,std::vector<std::size_t>> by_operand;
  for(std::size_t i=0;i<leaves.size();++i)if(leaves[i]->leaf&&merge_key(*leaves[i]->leaf)) {
    const auto o=operands(*leaves[i]->leaf);
    const auto lhs=o.whole.empty()?o.lhs:"col("+quote(leaves[i]->leaf->column)+")";  // an == range on ns
    if(!by_operand.count(lhs))order.push_back(lhs);
    by_operand[lhs].push_back(i);
  }
  if(order.empty())return node;
  std::vector<bool> merged(leaves.size(),false);std::vector<Expr> bounds;std::size_t before=0;
  for(const auto& lhs:order) {
    const auto& positions=by_operand[lhs];
    std::vector<std::pair<Leaf,MergeKey>> list;
    for(const auto i:positions){list.emplace_back(*leaves[i]->leaf,*merge_key(*leaves[i]->leaf));merged[i]=true;}
    const auto out=merge_interval(list);
    if(out.empty()){if(scan)empty=true;return node;}
    before+=positions.size();
    for(const auto& l:out)bounds.push_back(Expr{l,"",{}});
  }
  if(bounds.size()>=before)return node;
  std::vector<Expr> items;
  for(std::size_t i=0;i<leaves.size();++i)if(!merged[i])items.push_back(*leaves[i]);
  items.insert(items.end(),bounds.begin(),bounds.end());
  return fold_left(std::move(items));
}
// pending: the predicates above the node, topmost first.
Expr build_node(const std::vector<Expr>& pending,const Expr* own,bool scan,bool& empty) {
  std::map<std::string,Expr> groups;
  for(const auto& item:pending) {
    const auto key=predicate_column(item);
    const auto found=groups.find(key);
    if(found==groups.end()){groups.emplace(key,item);continue;}
    std::vector<const Expr*> present;conjuncts(found->second,present);
    const auto text=plan_text(item);
    if(item.leaf&&std::any_of(present.begin(),present.end(),[&](const Expr* e){return e->leaf&&plan_text(*e)==text;}))continue;
    found->second=binary(item,"&",std::move(found->second));
  }
  std::vector<Expr> entries;bool placed=own==nullptr;
  const auto own_key=own?predicate_column(*own):std::string();
  for(auto& [key,expr]:groups) {
    if(!placed&&key>own_key){entries.push_back(*own);placed=true;}
    entries.push_back(std::move(expr));
  }
  if(!placed)entries.push_back(*own);
  return merge_node(fold_left(std::move(entries)),scan,empty);
}
// Nodes bottom first; `empty`: the lowest node's interval is empty, so the
// frame is empty below every node (the lowest node is dropped).
std::vector<Expr> pushdown(const std::vector<Expr>& chain,bool& empty) {
  std::vector<Expr> nodes,pending;
  for(auto p=chain.rbegin();p!=chain.rend();++p) {
    if(p->leaf&&null_leaf(*p->leaf)){bool unused=false;nodes.push_back(build_node(pending,&*p,false,unused));pending.clear();}
    else pending.push_back(*p);
  }
  if(!pending.empty()) {
    auto node=build_node(pending,nullptr,true,empty);
    if(!empty)nodes.push_back(std::move(node));
  }
  std::reverse(nodes.begin(),nodes.end());return nodes;
}
std::optional<std::pair<std::string,std::string>> leaf_error(const Leaf& l) {
  namespace lit=detail::polars_lit;
  const auto& v=l.value;
  if(null(v))return {};
  if(collection(v)) {
    // Round CE: a Null column compares a list (cast to its type). Round CF:
    // but cannot order lists.
    if(l.dtype=="Null"&&l.op!="=="&&l.op!="!=") {
      try {
        const auto name=lit::physical_name(lit::list_of(lit::literal_of(lit::value_of(v)).type));
        return std::pair<std::string,std::string>{"InvalidOperationError","cannot perform '"+l.op+"' comparison between series '"+l.column+"' of dtype: "+name+" and series 'literal' of dtype: "+name};
      } catch(const PolarsSequenceFilterError&) {return {};}
    }
    if(l.dtype=="String"||l.dtype=="Null")return {};
    if(l.dtype.starts_with("Categorical")||l.dtype.starts_with("Enum(")) {
      // Round CD: a list never compares with a categorical column.
      try {
        const auto list=detail::polars_lit::literal_of(detail::polars_lit::value_of(v));
        return std::pair<std::string,std::string>{"ComputeError","cannot compare categorical with "+detail::polars_lit::short_name(detail::polars_lit::list_of(list.type))};
      } catch(const PolarsSequenceFilterError&) {return {};}
    }
    return std::pair<std::string,std::string>{"SchemaError","could not evaluate "+(l.op=="=="?std::string():"'"+l.op+"' ")+"comparison between series '"+l.column+"' of dtype: "+detail::polars_lit::is_in_dtype_name(l.dtype)+" and series 'literal' of dtype: List("+list_dtype(v)+")"};
  }
  if(wide_literal(v)) {
    if(l.dtype=="String")return std::pair<std::string,std::string>{"ComputeError","cannot compare string with numeric type (u64)"};
    if(l.dtype.starts_with("Categorical")||l.dtype.starts_with("Enum("))return std::pair<std::string,std::string>{"ComputeError","cannot compare categorical with u64"};
    return {};
  }
  const auto check=lit::check_scalar(l.column,l.dtype,lit::value_of(v),lit::compare_op(l.op),l.categories.empty()?nullptr:&l.categories);
  if(check.stage==lit::ScalarCheck::Stage::kCompute)return std::pair<std::string,std::string>{check.type,check.message};
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
  if(const auto u=wide_literal(l.value))return wide_truth(l,v,*u);
  namespace lit=detail::polars_lit;
  return lit::compare_scalar(std::optional<EventAttribute>(v),l.dtype,lit::value_of(l.value),lit::compare_op(l.op),l.categories.empty()?nullptr:&l.categories);
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
      const auto categories=enum_category_list(source,*column);
      each_leaf(node,[&](Leaf& l){l.dtype=dtype;l.categories=categories;});
      const Leaf* bad=nullptr;std::string message;
      each_leaf(node,[&](const Leaf& l) {
        if(bad||null(l.value))return;
        if(collection(l.value)) {
          if(l.dtype!="String")return;
          bad=&l;message="cannot cast List type (inner: '"+list_dtype(l.value)+"', to: 'String')";
          return;
        }
        if(wide_literal(l.value))return;
        namespace lit=detail::polars_lit;
        const auto check=lit::check_scalar(l.column,l.dtype,lit::value_of(l.value),lit::compare_op(l.op),l.categories.empty()?nullptr:&l.categories);
        if(check.stage==lit::ScalarCheck::Stage::kResolve){bad=&l;message=check.message;}
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
  bool empty=false;
  p.merged=pushdown(p.chain,empty);
  // Round CE: an empty interval in the lowest node empties the frame; the
  // filters above it still run (and raise) on the empty frame.
  if(empty)p.empty=true;
  for(const auto& node:p.merged) {
    const Expr* at=nullptr;
    if(const auto error=first_error(node,at))
      fail(error->first,phase(eager),error->second+"\n\nThis error occurred in the following expression:\n\t"+error_text(*at)+(node.leaf?"\n":"\nwhile evaluating this larger expression:\n\t"+error_text(node)+"\n"));
  }
  return p;
}
Predicate predicate(const EventTable& source,const PolarsAttributesFilterCall& call,bool eager,const std::vector<EventAttribute>* cells=nullptr) {
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
    const bool positive_now=trace||truth(call.options.positive);
    if(cells||collection(query)) {
      // Statistics filter on the column's own values as Python reads them.
      const auto value=cells?detail::polars_lit::Value::list(detail::polars_lit::python_values(*cells,p.dtype)):detail::polars_lit::value_of(query);
      p.literal=detail::polars_lit::literal_of(value);
      const bool empty_argument=cells?cells->empty():query.children.empty();
      if(!(positive_now&&empty_argument)) {
        try {
          detail::polars_lit::check_is_in(p.dtype,detail::polars_lit::enum_dtype(p.dtype)?enum_category_list(source,p.column):std::vector<std::string>{},*p.literal,
                                          suffix(filter_plan(source,list_membership(attr.value,p.dtype,*p.literal,positive_now,false,empty_argument)),true));
        } catch(const PolarsSequenceFilterError& error) {fail(error.type,phase(eager),error.message);}
      }
    } else if(text(query)) {
      // Round CH: the str names a column (checked above).
      const auto z=*index_of(source,query.value);
      const auto in=column_is_in(attr.value,p.dtype,query.value,source_dtype(source,z));
      if(!in.error.empty())fail("InvalidOperationError",phase(eager),in.error+suffix(filter_plan(source,membership_expr(attr.value,query,positive_now)),true));
      p.other=z;p.other_dtype=source_dtype(source,z);p.other_super=in.super;p.other_plan=in.lhs+".is_in(["+in.rhs+"])";
      // Round CH: a String side cast strictly to the other side's Enum fails
      // on a str outside its categories (the String column's name; the
      // imploded column has none).
      const bool cast_left=p.dtype=="String"&&detail::polars_lit::enum_dtype(p.other_dtype);
      const bool cast_right=detail::polars_lit::enum_dtype(p.dtype)&&p.other_dtype=="String";
      if(cast_left||cast_right) {
        const auto categories=enum_category_list(source,cast_left?z:p.column);
        const std::set<std::string> known(categories.begin(),categories.end());
        std::vector<std::string> failed;
        for(std::size_t i=0;i<source.size();++i){const auto w=cell(source,i,cast_left?p.column:z);if(!null(w)&&!known.count(w.value))failed.push_back(w.value);}
        if(!failed.empty())
          p.collect_error=detail::polars_lit::enum_conversion_message(cast_left?attr.value:std::string(),failed,source.size())+
                          "\n\nThis error occurred in the following expression:\n\t"+(cast_left?in.lhs:in.rhs)+
                          "\nwhile evaluating this larger expression:\n\t"+p.other_plan+(positive_now?"":".not()")+"\n";
      }
    } else {
    const auto qdtype=list_dtype(query);bool compatible=qdtype=="Null"||p.dtype=="Null";
    if(qdtype=="String")compatible=p.dtype=="String"||p.dtype.starts_with("Categorical")||p.dtype.starts_with("Enum(");
    else if(qdtype=="Int64"||qdtype=="Float64"||qdtype.starts_with("Unknown("))compatible=numeric_column(p.dtype)||p.dtype=="Null";
    else if(qdtype=="Boolean")compatible=p.dtype=="Boolean"||p.dtype=="Null";
    else if(qdtype=="Date")compatible=p.dtype=="Date"||p.dtype=="Null";
    else if(qdtype.starts_with("Datetime("))compatible=p.dtype.starts_with("Datetime(")||p.dtype=="Null";
    if(!compatible)fail("InvalidOperationError",phase(eager),"'is_in' cannot check for List("+qdtype+") values in "+membership_type(p.dtype)+" data"+suffix(filter_plan(source,membership_expr(attr.value,query,trace||truth(call.options.positive))),true));
    // Round CD: a datetime scalar checks its unit and zone like a list.
    if(qdtype.starts_with("Datetime(")&&p.dtype.starts_with("Datetime(")) {
      try {
        detail::polars_lit::check_is_in(p.dtype,{},detail::polars_lit::literal_of(detail::polars_lit::Value::list({detail::polars_lit::value_of(query)})),
                                        suffix(filter_plan(source,membership_expr(attr.value,query,trace||truth(call.options.positive))),true));
      } catch(const PolarsSequenceFilterError& error) {fail(error.type,phase(eager),error.message);}
    }
    if(!null(query))p.values.push_back(query);
    }
  }
  if(trace) {
    const auto& role=call.options.case_id_key;
    if(collection(role))fail("InvalidOperationError",phase(eager),"expected join keys/predicates"+(eager?"":resolved("'join'","UNIQUE[maintain_order: false, keep_strategy: Any] BY None\n  DF []; PROJECT */0 COLUMNS",{"'join'"})));
    // Round CE: a scalar value as the resolved plan coerces it.
    std::optional<std::string> scalar;
    if(!p.literal&&!null(*call.values)&&!collection(*call.values))scalar=detail::polars_lit::coerced_scalar_is_in(attr.value,p.dtype,detail::polars_lit::value_of(*call.values));
    const auto plan=filter_plan(source,p.literal?list_membership(attr.value,p.dtype,*p.literal,true,true,cells?cells->empty():call.values->children.empty()):scalar?*scalar:p.other?p.other_plan:membership_expr(attr.value,*call.values,true));
    if(integer(role))p.literal_case=true;
    else {
      const auto c=index_of(source,role.value);
      if(!c)fail("ColumnNotFoundError",phase(eager),column_prefix(source,role.value)+suffix(plan));
      p.case_column=c;p.case_dtype=source_dtype(source,*c);
    }
  }
  if(!p.collect_error.empty())fail("InvalidOperationError",phase(eager),p.collect_error);
  return p;
}
std::vector<bool> matching(const EventTable& source,const Predicate& p) {
  const auto op=p.call.operation;const bool positive=truth(p.call.options.positive);std::vector<bool> flags(source.size(),false);
  std::vector<EventAttribute> others;
  if(p.other)for(std::size_t j=0;j<source.size();++j){const auto w=cell(source,j,*p.other);if(!null(w))others.push_back(w);}
  for(std::size_t i=0;i<source.size();++i) {
    const auto v=cell(source,i,p.column);bool yes=false;
    if(numeric(op))yes=!p.empty&&std::all_of(p.merged.begin(),p.merged.end(),[&](const Expr& node){return truth_of(node,source,i)==1;});
    else {
      if(!null(v)) {
        if(p.other)yes=column_has(v,p.dtype,others,p.other_dtype,p.other_super);
        else if(p.literal)yes=detail::polars_lit::literal_has(*p.literal,v,p.dtype);
        else for(const auto& q:p.values) {
          if(null(q))continue;
          // Round CD: a datetime on a Datetime column is looked up as is_in
          // looks it up, by its microseconds whatever the zones.
          if(p.dtype.starts_with("Datetime(")&&date(q)&&!collection(q)) {
            namespace lit=detail::polars_lit;
            if(lit::literal_has(lit::literal_of(lit::Value::list({lit::value_of(q)})),v,p.dtype)){yes=true;break;}
            continue;
          }
          // Round CH: a number meets a Float32 column cast to Float32
          // (is_in([[v].cast(List(Float32))])), so 1e-5 finds the cell
          // 1e-5 rounded to Float32.
          if(p.dtype=="Float32"&&(real(q)||integer(q))&&!boolean(q)&&real(v)) {
            const double x=integer(q)?detail::polars_lit::float32_of_int(detail::polars_lit::value_of(q)):static_cast<double>(static_cast<float>(number(q))),y=number(v);
            if(x==y||(std::isnan(x)&&std::isnan(y))){yes=true;break;}
            continue;
          }
          if(scalar_equal(v,q)){yes=true;break;}
        }
        if(!positive&&!traced(op))yes=!yes;
      }
      else if(p.literal&&p.literal->values.empty()&&!positive&&!traced(op))yes=true;
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
  // Per-case counts group by [case, attribute]: the case key resolves first.
  const auto case_column=[&]{
    col_selector(options.case_id_key);
    if(collection(options.case_id_key))fail("IndexError","apply","index 0 is out of bounds for sequence of length 0");
    const auto at=index_of(source,options.case_id_key.value);
    if(!at)fail("ColumnNotFoundError","apply",column_prefix(source,options.case_id_key.value)+resolved("'group_by'",frame_plan(source),groupnodes));
    return at;
  };
  if(collection(attr)) {
    // Round CH: an empty key list fails on its first item only after the
    // per-case group_by resolved (a missing case key fails first).
    if(attr.children.empty()){if(result.once)(void)case_column();fail("IndexError","apply","tuple index out of range");}
    // A key list groups by each of its items.
    if(result.once)if(const auto error=detail::polars_lit::argument_literal_error(attr))fail("TypeError","apply",*error);
    for(const auto& item:attr.children)if(text(item)&&!index_of(source,item.value))
      if(!result.once)fail("ColumnNotFoundError","apply",column_prefix(source,item.value)+resolved("'group_by'",frame_plan(source),groupnodes));
    throw std::invalid_argument("Native Polars attribute counts accept a single key");
  }
  if(result.once)result.cases=case_column();
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
struct StatisticalSelection { EventAttribute attribute,values; bool unchanged=false,from_cache=false; };
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
  } else {counts=cache_counts(*call.count_cache);selection.from_cache=true;}
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
  return select_rows(source,matching(source,predicate(source,filtering,eager,selection.from_cache?nullptr:&selection.values.children)));
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
    // pl.col(attribute) is built while apply runs, so a bad selector fails there.
    if(call.operation==Op::kRelative&&!selected.unchanged)col_selector(selected.attribute);
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
