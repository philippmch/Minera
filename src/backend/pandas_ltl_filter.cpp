#include "minera/pandas_ltl_filter.hpp"
#include "pandas_parameter_carrier.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"
namespace minera { namespace {
[[noreturn]] void error(std::string kind,std::string message){throw PandasLtlFilterError(std::move(kind),std::move(message));}
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
    else error("ValueError", "pandas attributes selector has invalid UTF-8");
    for (unsigned i = 0; i < continuation; ++i) {
      if (position == value.size()) error("ValueError", "pandas attributes selector has short UTF-8");
      const auto next = static_cast<unsigned char>(value[position++]);
      if ((next & 0xc0) != 0x80) error("ValueError", "pandas attributes selector has invalid UTF-8 continuation");
      code = (code << 6) | (next & 63);
    }
    if ((continuation == 1 && code < 0x80) ||
        (continuation == 2 && code < 0x800) ||
        (continuation == 3 && code < 0x10000) || code > 0x10ffff)
      error("ValueError", "pandas attributes selector has invalid code point");
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

bool text_value(const EventAttribute& v){return !v.is_null&&v.container==AttributeContainerKind::kScalar&&(v.kind==AttributeKind::kString||v.kind==AttributeKind::kId)&&std::holds_alternative<std::monostate>(v.typed_value);}
bool date_value(const EventAttribute& v){return v.kind==AttributeKind::kDate||std::holds_alternative<NativeDate>(v.typed_value);}
bool numeric(const EventAttribute& v){return v.kind==AttributeKind::kInt||v.kind==AttributeKind::kFloat||v.kind==AttributeKind::kBoolean||std::holds_alternative<double>(v.typed_value)||std::holds_alternative<bool>(v.typed_value)||std::holds_alternative<std::int64_t>(v.typed_value)||std::holds_alternative<std::uint64_t>(v.typed_value);}
std::string type_name(const EventAttribute& v){
 if(v.is_null)return date_value(v)?"NaTType":"NoneType";
 switch(v.container){case AttributeContainerKind::kList:return "list";case AttributeContainerKind::kTuple:return "tuple";case AttributeContainerKind::kContainer:return "dict";case AttributeContainerKind::kSet:return "set";case AttributeContainerKind::kFrozenSet:return "frozenset";default:break;}
 if(is_native_na_attribute(v))return "NAType";
 if(date_value(v))return std::holds_alternative<NativeDate>(v.typed_value)?"Timestamp":"datetime.datetime";
 if(v.kind==AttributeKind::kBytes)return "bytes";
 if(v.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(v.typed_value))return "bool";
 if(v.kind==AttributeKind::kInt||std::holds_alternative<std::int64_t>(v.typed_value)||std::holds_alternative<std::uint64_t>(v.typed_value))return "int";
 return numeric(v)?"float":"str";
}
std::string repr(const EventAttribute& v){
 if(text_value(v))return string_key_repr(v.value);
 return root_cause_attribute_category(v);
}
bool truth(const EventAttribute& v){
 if(v.is_null)return false;
 if(is_native_na_attribute(v))error("TypeError","boolean value of NA is ambiguous");
 if(v.container!=AttributeContainerKind::kScalar)return !v.children.empty();
 if(numeric(v))return native_attribute_number(v)!=0;
 return !v.value.empty();
}
std::optional<ParsedExactTimestamp> instant(const EventAttribute& v){
 if(v.is_null||is_native_na_attribute(v))return std::nullopt;
 const auto* date=std::get_if<NativeDate>(&v.typed_value);
 return parse_timestamp_exact(date?date->value:v.value);
}
bool missing(const EventAttribute& v){
 if(v.is_null||is_native_na_attribute(v))return true;
 if(numeric(v))return std::isnan(native_attribute_number(v));
 return date_value(v)&&!instant(v);
}

std::string identity(const EventAttribute& value){
 if(value.container!=AttributeContainerKind::kScalar){
  std::string result="C"+std::to_string(static_cast<int>(value.container))+":";
  for(const auto& child:value.children){const auto part=identity(child);result+=std::to_string(part.size())+":"+part;}
  return result;
 }
 if(is_native_na_attribute(value))return "NA";
 if(value.is_null)return date_value(value)?"NaT":"None";
 if(date_value(value)){
  const auto parsed=instant(value);if(!parsed)return "NaT";
  return std::string(parsed->timezone_aware?"DA":"DN")+std::to_string(parsed->seconds)+":"+std::to_string(parsed->nanoseconds);
 }
 if(numeric(value)&&std::isnan(native_attribute_number(value)))return "NaN";
 return root_cause_scalar_key_fingerprint(value);
}
std::string group_key(const EventAttribute& value){
 if(!root_cause_key_is_hashable(value))error("TypeError","unhashable type: '"+type_name(value)+"'");
 return identity(value);
}
bool arithmetic_index(const std::vector<EventTableIndexLabel>& labels){
 std::vector<TokenInteger> values;
 for(const auto& label:labels){const auto* v=std::get_if<EventAttribute>(&label.value);if(!v||v->is_null)return false;
  if(const auto* n=std::get_if<std::int64_t>(&v->typed_value))values.emplace_back(*n);
  else if(const auto* n=std::get_if<std::uint64_t>(&v->typed_value))values.emplace_back(*n);
  else return false;
 }
 if(values.size()<2)return true;
 const auto step=values[1]-values[0];if(step==0)return false;
 for(std::size_t i=2;i<values.size();++i)if(values[i]-values[i-1]!=step)return false;
 return true;
}
EventTable subset(const EventTable& source,const std::vector<bool>& keep){
 EventTable result=source;result.rows.clear();
 if(result.index)result.index->labels.clear();else result.index=EventTableIndex{"RangeIndex","int64",{std::nullopt},{}};
 for(std::size_t i=0;i<source.size();++i)if(keep[i]){
  result.rows.push_back(source.rows[i]);
  if(source.index)result.index->labels.push_back(source.index->labels[i]);
  else {EventAttribute label{"",std::to_string(i),AttributeKind::kInt};label.typed_value=static_cast<std::uint64_t>(i);result.index->labels.push_back({std::move(label)});}
 }
 if(result.index->kind=="RangeIndex"&&!arithmetic_index(result.index->labels))result.index->kind="Index";
 return result;
}

const EventAttribute& cell(const EventTable& table,std::size_t row,std::size_t column){
 static const EventAttribute absent=[](){EventAttribute value{"",""};value.is_null=true;return value;}();
 const auto& value=table.rows[row][column];return value?*value:absent;
}
enum class Rule { kEventually, kNext, kFourEyes, kDifferent };
EventAttribute scalar(std::string value){return EventAttribute{"",std::move(value)};}
std::vector<EventAttribute> iterable(const EventAttribute& value){
 if(value.is_null)error("TypeError","'NoneType' object is not iterable");
 if(value.container==AttributeContainerKind::kContainer){std::vector<EventAttribute> result;for(const auto& child:value.children)result.push_back(scalar(child.key));return result;}
 if(value.container!=AttributeContainerKind::kScalar)return value.children;
 if(text_value(value)){
  std::vector<EventAttribute> result;
  for(std::size_t i=0;i<value.value.size();){const auto begin=i++;while(i<value.value.size()&&(static_cast<unsigned char>(value.value[i])&0xc0)==0x80)++i;result.push_back(scalar(value.value.substr(begin,i-begin)));}
  return result;
 }
 error("TypeError","'"+type_name(value)+"' object is not iterable");
}
std::size_t length(const EventAttribute& value){
 if(value.is_null||is_native_na_attribute(value)||(value.container==AttributeContainerKind::kScalar&&!text_value(value)))error("TypeError","object of type '"+type_name(value)+"' has no len()");
 return iterable(value).size();
}
EventAttribute element(const EventAttribute& value,std::size_t position){
 if(value.container==AttributeContainerKind::kScalar&&!text_value(value))error("TypeError","'"+type_name(value)+"' object is not subscriptable");
 const auto values=iterable(value);
 if(position>=values.size())error("IndexError","list index out of range");
 return values[position];
}
bool equal_value(const EventAttribute& a,const EventAttribute& b){return identity(a)==identity(b);}
bool matches(const EventAttribute& value,const EventAttribute& target){return !missing(value)&&!missing(target)&&equal_value(value,target);}
void check_target(const EventAttribute& target,std::size_t rows,const std::string& dtype){
 if(target.container==AttributeContainerKind::kList&&target.children.size()!=rows)
  error("ValueError","('Lengths must match to compare', ("+std::to_string(rows)+",), ("+std::to_string(target.children.size())+",))");
 // A str column compares a tuple as an array.
 if(target.container==AttributeContainerKind::kTuple&&dtype=="str"){
  if(target.children.size()!=rows)error("ValueError","Lengths of operands do not match: "+std::to_string(rows)+" != "+std::to_string(target.children.size()));
  if(rows)error("ValueError","ltl filter vector targets are outside the captured native domain");
 }
}
std::vector<std::size_t> selected_columns(const EventTable& source,const std::vector<EventAttribute>& requested){
 std::vector<std::size_t> result;std::vector<std::string> absent;std::vector<std::string> names;std::size_t missing_count=0;
 for(const auto& key:requested){
  names.push_back(key.is_null?"nan":repr(key));
  const auto found=text_value(key)?std::find(source.columns.begin(),source.columns.end(),key.value):source.columns.end();
  if(found==source.columns.end()){++missing_count;if(std::find(absent.begin(),absent.end(),names.back())==absent.end())absent.push_back(names.back());}else result.push_back(static_cast<std::size_t>(found-source.columns.begin()));
 }
 const auto list=[](const std::vector<std::string>& values){std::string output="[";for(std::size_t i=0;i<values.size();++i){if(i)output+=", ";output+=values[i];}return output+"]";};
 if(!absent.empty()){
  const auto message=missing_count==requested.size()?"None of [Index("+list(names)+", dtype='str')] are in the [columns]":list(absent)+" not in index";
  error("KeyError",string_key_repr(message));
 }
 return result;
}
PandasLtlFilterOptions resolve_ltl_options(const std::vector<PandasLtlFilterParameter>& parameters){
 PandasLtlFilterOptions result;
 const auto assign=[&](const EventAttribute& v){
  if(v.key=="pm4py:param:case_id_key")result.case_id_key=v;
  else if(v.key=="pm4py:param:attribute_key")result.attribute_key=v;
  else if(v.key=="pm4py:param:timestamp_key")result.timestamp_key=v;
  else if(v.key=="pm4py:param:resource_key")result.resource_key=v;
  else if(v.key=="positive")result.positive=v;
  else if(v.key=="enable_timestamp")result.enable_timestamp=v;
  else if(v.key=="timestamp_diff_boundaries")result.timestamp_diff_boundaries=v;
 };
 for(const auto& p:parameters)if(!p.enum_key)assign(p.value);
 for(const auto& p:parameters)if(p.enum_key)assign(p.value);
 return result;
}
PandasLtlFilterOptions resolve_ltl_options(const EventAttribute& parameters){
 if(parameters.is_null||(parameters.container==AttributeContainerKind::kList&&parameters.children.empty()))return {};
 if(parameters.container==AttributeContainerKind::kContainer){std::vector<PandasLtlFilterParameter> result;for(const auto& child:parameters.children)result.push_back({child,false});return resolve_ltl_options(result);}
 detail::check_parameter_carrier(parameters,[](const std::string& type,const std::string& message){error(type,message);});
 return {};
}
double duration(const EventAttribute& a,const EventAttribute& b){
 const auto x=instant(a),y=instant(b);
 if(!x||!y)return std::numeric_limits<double>::quiet_NaN();
 const auto ns=(TokenInteger(y->seconds)-TokenInteger(x->seconds))*TokenInteger(1'000'000'000)+(TokenInteger(y->nanoseconds)-TokenInteger(x->nanoseconds));
 return static_cast<double>(ns)/1e9;
}
bool temporal_bound(double value,const EventAttribute& bound,bool lower){
 if(missing(bound))return false;
 if(!numeric(bound))error("TypeError","Invalid comparison between dtype=float64 and "+type_name(bound));
 const auto number=native_attribute_number(bound);
 return lower?value>=number:value<=number;
}
EventTable apply_rule(const EventTable& source,std::vector<EventAttribute> anchors,const PandasLtlFilterOptions& options,Rule rule,const EventAttribute* pattern=nullptr){
 EventAttribute empty;empty.container=AttributeContainerKind::kList;
 const auto& boundaries=options.timestamp_diff_boundaries?*options.timestamp_diff_boundaries:empty;
 bool timed=false;std::size_t boundary_count=0;
 if(rule==Rule::kEventually){boundary_count=length(boundaries);timed=options.enable_timestamp?truth(*options.enable_timestamp):boundary_count>0;}
 std::vector<EventAttribute> roles{options.case_id_key,options.attribute_key};
 const bool resources=rule==Rule::kFourEyes||rule==Rule::kDifferent;
 if(resources)roles.push_back(options.resource_key);
 if(timed)roles.push_back(options.timestamp_key);
 const auto columns=selected_columns(source,roles);
 // The eventually-follows pattern is iterated after the column selection.
 if(pattern)anchors=iterable(*pattern);
 // A key naming the same column as another role duplicates it in the
 // reference's selections.
 if(columns[0]==columns[1]||(resources&&columns[1]==columns[2]))error("ValueError","cannot reindex on an axis with duplicate labels");
 if(resources&&columns[0]==columns[2])error("ValueError","Cannot set a DataFrame with multiple columns to the single column @@conceptname");
 std::vector<std::size_t> valid;
 for(std::size_t i=0;i<source.size();++i){bool good=true;for(const auto c:columns)if(missing(cell(source,i,c))){good=false;break;}if(good)valid.push_back(i);}
 for(const auto& target:anchors)check_target(target,source.size(),source.dtypes.empty()?"object":source.dtypes[columns[1]]);
 if(rule==Rule::kEventually&&anchors.size()<2)error("IndexError","list index out of range");
 if(timed){
  const auto dtype=source.dtypes.empty()?"object":source.dtypes[columns.back()];
  if(!dtype.starts_with("datetime64")){
   bool strings=false;for(const auto row:valid)strings|=text_value(cell(source,row,columns.back()));
   if(strings)error("TypeError","unsupported operand type(s) for -: 'str' and 'str'");
   error("AttributeError","Can only use .dt accessor with datetimelike values");
  }
  // Each step's bounds are compared eagerly, whether or not rows remain.
  if(rule==Rule::kEventually&&boundary_count){
   for(std::size_t i=0;i+1<anchors.size();++i){
    const auto bound=element(boundaries,i);
    temporal_bound(0.0,element(bound,0),true);temporal_bound(0.0,element(bound,1),false);
   }
  }
 }
 std::unordered_map<std::string,std::vector<std::size_t>> groups;
 std::vector<std::string> group_order;
 for(const auto row:valid){
  const auto& value=cell(source,row,columns[0]);
  auto key=group_key(value);
  // Four Eyes retains separate scalar representations while forming resource
  // sets, then assigns their result to normal equal-case membership in order.
  if(rule==Rule::kFourEyes)key=type_name(value)+":"+key;
  if(!groups.contains(key))group_order.push_back(key);
  groups[key].push_back(row);
 }
 std::unordered_set<std::string> selected,eligible;
 const auto& activity_column=columns[1];
 for(const auto& key:group_order){
  const auto& rows=groups.at(key);
  if(rule==Rule::kEventually){
   const auto search=[&](auto&& self,std::size_t depth,std::size_t begin,std::size_t previous)->bool{
    if(depth==anchors.size())return true;
    for(std::size_t i=begin;i<rows.size();++i){const auto row=rows[i];
     if(!matches(cell(source,row,activity_column),anchors[depth]))continue;
     if(depth&&timed&&boundary_count){
      const auto bound=element(boundaries,depth-1);
      const auto elapsed=duration(cell(source,previous,columns.back()),cell(source,row,columns.back()));
      if(!temporal_bound(elapsed,element(bound,0),true)||!temporal_bound(elapsed,element(bound,1),false))continue;
     }
     if(self(self,depth+1,i+1,row))return true;
    }
    return false;
   };
   if(search(search,0,0,0))selected.insert(key);
  }else if(resources){
   std::vector<const EventAttribute*> a,b;
   for(const auto row:rows){if(matches(cell(source,row,activity_column),anchors[0]))a.push_back(&cell(source,row,columns[2]));if(rule==Rule::kFourEyes&&matches(cell(source,row,activity_column),anchors[1]))b.push_back(&cell(source,row,columns[2]));}
   if(rule==Rule::kDifferent){bool different=false;for(const auto* x:a)for(const auto* y:a)if(!equal_value(*x,*y))different=true;if(different)selected.insert(key);}
   else {
    const auto member=identity(cell(source,rows.front(),columns[0]));
    selected.erase(member);eligible.erase(member);
    if(!a.empty()&&!b.empty()){
     // The reference pairs every A event with the case's last B event only.
     eligible.insert(member);bool overlap=false;
     for(const auto* x:a)if(equal_value(*x,*b.back()))overlap=true;
     if(!overlap)selected.insert(member);
    }
   }
  }
 }
 if(rule==Rule::kNext){
  // Public NEXT uses the final B and C occurrences, including later invalidating
  // occurrences after an earlier ABC. A must directly precede that final B.
  for(const auto& [key,rows]:groups){
   std::optional<std::size_t> b,c;
   for(const auto row:rows){
    if(matches(cell(source,row,activity_column),anchors[1]))b=row;
    if(matches(cell(source,row,activity_column),anchors[2]))c=row;
   }
   if(!b||!c||*b==0||*c!=*b+1)continue;
   const auto a=*b-1;
   if(!missing(cell(source,a,columns[0]))&&!missing(cell(source,a,activity_column))&&
      key==group_key(cell(source,a,columns[0]))&&matches(cell(source,a,activity_column),anchors[0]))selected.insert(key);
  }
 }
 const bool positive=truth(options.positive);
 std::vector<bool> keep(source.size());
 for(std::size_t i=0;i<source.size();++i){const auto key=identity(cell(source,i,columns[0]));const bool member=selected.contains(key);keep[i]=positive?member:rule==Rule::kFourEyes?eligible.contains(key)&&!member:!member;}
 return subset(source,keep);
}
} // namespace
EventTable filter_pandas_ltl_eventually_follows(const EventTable& source,const EventAttribute& pattern,const PandasLtlFilterOptions& options){return apply_rule(source,{},options,Rule::kEventually,&pattern);}
EventTable filter_pandas_ltl_next(const EventTable& source,const EventAttribute& a,const EventAttribute& b,const EventAttribute& c,const PandasLtlFilterOptions& options){return apply_rule(source,{a,b,c},options,Rule::kNext);}
EventTable filter_pandas_ltl_four_eyes(const EventTable& source,const EventAttribute& a,const EventAttribute& b,const PandasLtlFilterOptions& options){return apply_rule(source,{a,b},options,Rule::kFourEyes);}
EventTable filter_pandas_ltl_different_persons(const EventTable& source,const EventAttribute& a,const PandasLtlFilterOptions& options){return apply_rule(source,{a},options,Rule::kDifferent);}
#define MINERA_PANDAS_LTL_WRAPPER_1(name) \
EventTable name(const EventTable& s,const EventAttribute& a,const EventAttribute& p){return name(s,a,resolve_ltl_options(p));} \
EventTable name(const EventTable& s,const EventAttribute& a,const std::vector<PandasLtlFilterParameter>& p){return name(s,a,resolve_ltl_options(p));}
MINERA_PANDAS_LTL_WRAPPER_1(filter_pandas_ltl_eventually_follows)
MINERA_PANDAS_LTL_WRAPPER_1(filter_pandas_ltl_different_persons)
#undef MINERA_PANDAS_LTL_WRAPPER_1
EventTable filter_pandas_ltl_four_eyes(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const EventAttribute& p){return filter_pandas_ltl_four_eyes(s,a,b,resolve_ltl_options(p));}
EventTable filter_pandas_ltl_four_eyes(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const std::vector<PandasLtlFilterParameter>& p){return filter_pandas_ltl_four_eyes(s,a,b,resolve_ltl_options(p));}
EventTable filter_pandas_ltl_next(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const EventAttribute& c,const EventAttribute& p){return filter_pandas_ltl_next(s,a,b,c,resolve_ltl_options(p));}
EventTable filter_pandas_ltl_next(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const EventAttribute& c,const std::vector<PandasLtlFilterParameter>& p){return filter_pandas_ltl_next(s,a,b,c,resolve_ltl_options(p));}
} // namespace minera
