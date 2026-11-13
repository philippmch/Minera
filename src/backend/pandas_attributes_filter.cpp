#include "minera/pandas_attributes_filter.hpp"
#include "pandas_parameter_carrier.hpp"
#include "pandas_numpy_view.hpp"
#include <algorithm>
#include <cctype>
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
[[noreturn]] void error(std::string kind,std::string message){throw PandasAttributesFilterError(std::move(kind),std::move(message));}
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
 // A date without a native payload is a Python datetime argument.
 if(date_value(v))return std::holds_alternative<NativeDate>(v.typed_value)?"Timestamp":"datetime";
 if(v.kind==AttributeKind::kBytes)return "bytes";
 if(v.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(v.typed_value))return "bool";
 if(v.kind==AttributeKind::kInt||std::holds_alternative<std::int64_t>(v.typed_value)||std::holds_alternative<std::uint64_t>(v.typed_value))return "int";
 return numeric(v)?"float":"str";
}
std::string repr(const EventAttribute& v){
 if(text_value(v))return string_key_repr(v.value);
 return root_cause_attribute_category(v);
}
// The type's tp_name, as Python's comparison errors report it.
std::string qualified_type_name(const EventAttribute& v){const auto name=type_name(v);return name=="datetime"?"datetime.datetime":name;}
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

EventTable masked_frame(const EventTable& source){
 EventTable result=source;
 // Masking no row keeps every dtype.
 if(source.size()==0)return result;
 for(std::size_t c=0;c<source.columns.size();++c){
  const auto dtype=source.dtypes.empty()?"object":source.dtypes[c];
  const bool nullable=dtype=="boolean"||dtype=="string"||dtype=="Int64"||dtype=="UInt64"||dtype=="Float64";
  const bool date=dtype.starts_with("datetime64");
  if(!result.dtypes.empty()&&(dtype=="int64"||dtype=="uint64"))result.dtypes[c]="float64";
  if(!result.dtypes.empty()&&dtype=="bool")result.dtypes[c]="object";
  for(auto& row:result.rows){
   EventAttribute value{source.columns[c],"nan",AttributeKind::kFloat};value.typed_value=std::numeric_limits<double>::quiet_NaN();
   if(nullable)value=make_native_na_attribute(source.columns[c]);
   else if(date){value=EventAttribute{source.columns[c],"NaT",AttributeKind::kDate,true};value.typed_value=NativeDate{"NaT",NativeDateResolution::kNanosecond};}
   row[c]=std::move(value);
  }
 }
 return result;
}
std::optional<std::size_t> column(const EventTable& source,const EventAttribute& key){
 if(!text_value(key))return std::nullopt;
 const auto found=std::find(source.columns.begin(),source.columns.end(),key.value);
 return found==source.columns.end()?std::nullopt:std::optional<std::size_t>(found-source.columns.begin());
}
bool empty_list(const EventAttribute& v){return v.container==AttributeContainerKind::kList&&v.children.empty();}
// df[[...]]: every listed label must be a column.
void check_list_columns(const EventTable& source,const EventAttribute& key){
 std::vector<std::string> names,absent;bool strings=true,integers=true;std::size_t missing=0;
 for(const auto& item:key.children){
  names.push_back(repr(item));strings&=text_value(item);integers&=type_name(item)=="int";
  if(!column(source,item)){++missing;if(std::find(absent.begin(),absent.end(),names.back())==absent.end())absent.push_back(names.back());}
 }
 const auto list=[](const std::vector<std::string>& values){std::string output="[";for(std::size_t i=0;i<values.size();++i){if(i)output+=", ";output+=values[i];}return output+"]";};
 if(absent.empty())return;
 // Every label missing (a label named twice counts twice): pandas lists
 // the whole index; otherwise each missing label once (round BA).
 if(missing==names.size()){
  const std::string dtype=names.empty()?"object":strings?"str":integers?"int64":"object";
  error("KeyError",string_key_repr("None of [Index("+list(names)+", dtype='"+dtype+"')] are in the [columns]"));
 }
 error("KeyError",string_key_repr(list(absent)+" not in index"));
}
// repr() of a Python datetime argument (its text is ISO with an optional
// fraction and UTC offset).
std::string key_repr(const EventAttribute& key){
 if(!date_value(key)||key.is_null||std::holds_alternative<NativeDate>(key.typed_value))return repr(key);
 const auto& text=key.value;
 const auto number=[&](std::size_t at,std::size_t width){return std::stoi(text.substr(at,width));};
 std::string result="datetime.datetime("+std::to_string(number(0,4))+", "+std::to_string(number(5,2))+", "+std::to_string(number(8,2))+", "+std::to_string(number(11,2))+", "+std::to_string(number(14,2));
 const int second=number(17,2);std::size_t position=19;int micro=0;
 if(position<text.size()&&text[position]=='.'){std::string digits;++position;while(position<text.size()&&std::isdigit(static_cast<unsigned char>(text[position])))digits+=text[position++];digits.resize(6,'0');micro=std::stoi(digits.substr(0,6));}
 if(second||micro)result+=", "+std::to_string(second);
 if(micro)result+=", "+std::to_string(micro);
 if(position<text.size()){
  const int sign=text[position]=='-'?-1:1;const int minutes=sign*(number(position+1,2)*60+number(position+4,2));
  if(minutes==0)result+=", tzinfo=datetime.timezone.utc";
  else {const int seconds=minutes*60;result+=", tzinfo=datetime.timezone(datetime.timedelta("+std::string(seconds<0?"days=-1, seconds="+std::to_string(86400+seconds):"seconds="+std::to_string(seconds))+"))";}
 }
 return result+")";
}
std::size_t direct_column(const EventTable& source,const EventAttribute& key){
 if(const auto found=column(source,key))return *found;
 if(key.container==AttributeContainerKind::kList)check_list_columns(source,key);
 error("KeyError",key_repr(key));
}
std::size_t case_column(const EventTable& source,const EventAttribute& key){
 if(empty_list(key))error("ValueError","Length mismatch: Expected axis has "+std::to_string(source.size())+" elements, new values have 0 elements");
 if(const auto found=column(source,key))return *found;
 error("KeyError",string_key_repr("None of ["+repr(key)+"] are in the columns"));
}
const EventAttribute& cell(const EventTable& source,std::size_t row,std::size_t col){
 static const EventAttribute none{"","",AttributeKind::kString,true};
 return source.rows[row][col]?*source.rows[row][col]:none;
}
std::vector<EventAttribute> admitted_values(const EventAttribute& values,bool allow_none=false){
 if(values.is_null&&allow_none)return {};
 if(values.container==AttributeContainerKind::kContainer){std::vector<EventAttribute> result;for(const auto& value:values.children)result.push_back(EventAttribute{"",value.key});return result;}
 if(values.container!=AttributeContainerKind::kScalar)return values.children;
 error("TypeError","only list-like objects are allowed to be passed to isin(), you passed a `"+type_name(values)+"`");
}
bool member(const EventAttribute& value,const std::vector<EventAttribute>& admitted){
 const auto key=identity(value);return std::any_of(admitted.begin(),admitted.end(),[&](const auto& v){return identity(v)==key;});
}
// Series.isin: in a str column a null cell matches a None or NaN value.
bool isin(const EventTable& source,std::size_t col,const EventAttribute& value,const std::vector<EventAttribute>& admitted){
 if(missing(value)&&col<source.dtypes.size()&&source.dtypes[col]=="str")
  return std::any_of(admitted.begin(),admitted.end(),[](const auto& v){const auto key=identity(v);return key=="None"||key=="NaN";});
 return member(value,admitted);
}
EventTable project_cases(const EventTable& source,const EventAttribute& case_key,const std::vector<bool>& selected,bool positive){
 const auto cases=case_column(source,case_key);std::unordered_set<std::string> ids;
 for(std::size_t i=0;i<source.size();++i)if(selected[i])ids.insert(identity(cell(source,i,cases)));
 // Index.isin on a masked numeric index (Int64, Float64, ...) never matches
 // a <NA> label, even when the selected cases hold one (round BZ).
 const bool masked=cases<source.dtypes.size()&&detail::masked_numeric_dtype(source.dtypes[cases]);
 std::vector<bool> keep(source.size());
 for(std::size_t i=0;i<source.size();++i){
  const auto& id=cell(source,i,cases);
  keep[i]=(!(masked&&(id.is_null||is_native_na_attribute(id)))&&ids.contains(identity(id)))==positive;
 }
 return subset(source,keep);
}
EventTable values_filter(const EventTable& source,const EventAttribute& values,const PandasAttributesFilterOptions& options,bool events){
 source.validate();
 if(empty_list(options.attribute_key)){
  if(!values.is_null&&values.container==AttributeContainerKind::kScalar)error("TypeError","only list-like or dict-like objects are allowed to be passed to DataFrame.isin(), you passed a '"+type_name(values)+"'");
  return events?masked_frame(source):project_cases(source,options.case_id_key,std::vector<bool>(source.size()),truth(options.positive));
 }
 const auto attribute=direct_column(source,options.attribute_key);
 const auto admitted=admitted_values(values,!events);
 const bool positive=truth(options.positive),keep_nan=truth(options.keep_nan_values);
 std::vector<bool> selected(source.size());
 for(std::size_t i=0;i<source.size();++i){const auto& v=cell(source,i,attribute);const bool match=isin(source,attribute,v,admitted);selected[i]=events?((match||(keep_nan&&missing(v)))==positive):(match||(keep_nan&&missing(v)));}
 return events?subset(source,selected):project_cases(source,options.case_id_key,selected,positive);
}
EventAttribute float32_bound(const EventAttribute& bound){
 if(!numeric(bound)||missing(bound))return bound;
 EventAttribute result=bound;result.kind=AttributeKind::kFloat;result.typed_value=static_cast<double>(static_cast<float>(native_attribute_number(bound)));return result;
}
// op is one of ">=", "<=", "<", ">" (value op bound).
// Per-row bounds. A tuple is an array operand: a str column needs equal
// lengths, other columns broadcast one item (or one row) with numpy.
std::vector<EventAttribute> row_bounds(const EventAttribute& bound,std::size_t rows,const std::string& dtype){
 if(text_value(bound)&&dtype!="object"&&dtype!="str")error("TypeError","Invalid comparison between dtype="+dtype+" and str");
 if(bound.container==AttributeContainerKind::kList){
  if(bound.children.size()!=rows)error("ValueError","('Lengths must match to compare', ("+std::to_string(rows)+",), ("+std::to_string(bound.children.size())+",))");
  return bound.children;
 }
 if(bound.container!=AttributeContainerKind::kTuple)return std::vector<EventAttribute>(rows,bound);
 const auto k=bound.children.size();
 if(dtype=="str"){
  if(k!=rows)error("ValueError","Lengths of operands do not match: "+std::to_string(rows)+" != "+std::to_string(k));
  return bound.children;
 }
 if(k==1)return std::vector<EventAttribute>(rows,bound.children.front());
 if(k==rows)return bound.children;
 if(rows==1)error("ValueError","Length of values ("+std::to_string(k)+") does not match length of index (1)");
 error("ValueError","operands could not be broadcast together with shapes ("+std::to_string(rows)+",) ("+std::to_string(k)+",) ");
}
bool comparison(const EventAttribute& value,const EventAttribute& bound,const std::string& op,const std::string& dtype){
 if(missing(value)||missing(bound))return false;
 int order=0;
 if(text_value(value)&&text_value(bound))order=value.value<bound.value?-1:bound.value<value.value?1:0;
 else if(date_value(value)&&date_value(bound)){
  const auto a=instant(value),b=instant(bound);if(!a||!b)return false;
  order=a->seconds==b->seconds?(a->nanoseconds<b->nanoseconds?-1:a->nanoseconds>b->nanoseconds?1:0):(a->seconds<b->seconds?-1:1);
 }else if(numeric(value)&&numeric(bound))order=native_scalar_key_compare(value,dtype=="float32"?float32_bound(bound):bound);
 else error("TypeError","'"+op+"' not supported between instances of '"+qualified_type_name(value)+"' and '"+qualified_type_name(bound)+"'");
 return op==">="?order>=0:op=="<="?order<=0:op=="<"?order<0:order>0;
}
EventTable numeric_filter(const EventTable& source,const EventAttribute& lower,const EventAttribute& upper,const PandasAttributesFilterOptions& options,bool events){
 source.validate();
 if(empty_list(options.attribute_key)){
  if(events)return masked_frame(source);
  // The masked frame leaves every case id null, so the null cases match.
  const auto cases=case_column(source,options.case_id_key);std::vector<bool> nulls(source.size());
  for(std::size_t i=0;i<source.size();++i)nulls[i]=missing(cell(source,i,cases));
  return project_cases(source,options.case_id_key,nulls,truth(options.positive));
 }
 const auto attribute=direct_column(source,options.attribute_key);
 const bool positive=truth(options.positive),keep_nan=truth(options.keep_nan_values);
 const auto dtype=source.dtypes.empty()?"object":source.dtypes[attribute];
 std::vector<bool> low(source.size()),high(source.size()),selected(source.size());
 // Pandas evaluates complete vector comparisons before combining the masks;
 // negative event filtering keeps (value < lower) | (value > upper).
 const bool outside=events&&!positive;
 const auto lows=row_bounds(lower,source.size(),dtype);
 for(std::size_t i=0;i<source.size();++i)low[i]=comparison(cell(source,i,attribute),lows[i],outside?"<":">=",dtype);
 const auto highs=row_bounds(upper,source.size(),dtype);
 for(std::size_t i=0;i<source.size();++i)high[i]=comparison(cell(source,i,attribute),highs[i],outside?">":"<=",dtype);
 for(std::size_t i=0;i<source.size();++i){const auto& value=cell(source,i,attribute);const bool within=low[i]&&high[i];selected[i]=events?((outside?(low[i]||high[i]):within)||(keep_nan&&missing(value))):(within||(keep_nan&&missing(value)));}
 if(!events){
  for(const auto& control:{std::pair{&options.stream_filter_key1,&options.stream_filter_value1},std::pair{&options.stream_filter_key2,&options.stream_filter_value2}}){
   if(!*control.first||(*control.first)->is_null)continue;
   // A list key compares a sub-frame, which keeps no row.
   if((*control.first)->container==AttributeContainerKind::kList){check_list_columns(source,**control.first);std::fill(selected.begin(),selected.end(),false);continue;}
   const auto stream=direct_column(source,**control.first);
   // Each stream filter compares the rows that remain; a list value needs one
   // item per remaining row.
   static const EventAttribute none{"","",AttributeKind::kString,true};
   const auto& value=*control.second?**control.second:none;
   std::vector<std::size_t> remaining;for(std::size_t i=0;i<source.size();++i)if(selected[i])remaining.push_back(i);
   if(value.container==AttributeContainerKind::kList&&value.children.size()!=remaining.size())
    error("ValueError","('Lengths must match to compare', ("+std::to_string(remaining.size())+",), ("+std::to_string(value.children.size())+",))");
   if(value.container==AttributeContainerKind::kTuple&&!source.dtypes.empty()&&source.dtypes[stream]=="str"){
    if(value.children.size()!=remaining.size())error("ValueError","Lengths of operands do not match: "+std::to_string(remaining.size())+" != "+std::to_string(value.children.size()));
    if(!remaining.empty())error("ValueError","attribute stream tuples are outside the captured native domain");
   }
   for(std::size_t k=0;k<remaining.size();++k){
    const auto& item=value.container==AttributeContainerKind::kList?value.children[k]:value;const auto& v=cell(source,remaining[k],stream);
    selected[remaining[k]]=!missing(item)&&!missing(v)&&identity(v)==identity(item);
   }
  }
 }
 return events?subset(source,selected):project_cases(source,options.case_id_key,selected,positive);
}
PandasAttributesCountMap counts(const EventTable& source,const EventAttribute& key){
 if(empty_list(key))error("ValueError","No group keys passed!");
 const auto attribute=direct_column(source,key);PandasAttributesCountMap result;
 std::unordered_map<std::string,std::size_t> positions;
 for(std::size_t i=0;i<source.size();++i){const auto& value=cell(source,i,attribute);if(missing(value))continue;
  const auto id=group_key(value);const auto [position,added]=positions.emplace(id,result.entries.size());
  if(added)result.entries.push_back({value,EventAttribute{"","0",AttributeKind::kInt}});
  auto& count=result.entries[position->second].second;const auto n=std::stoull(count.value)+1;count.value=std::to_string(n);count.typed_value=static_cast<std::uint64_t>(n);
 }
 // value_counts is descending and stable for equal observed counts.
 std::stable_sort(result.entries.begin(),result.entries.end(),[](const auto& a,const auto& b){return native_scalar_key_compare(a.second,b.second)>0;});
 return result;
}
bool greater_equal(const EventAttribute& left,const EventAttribute& right){
 if(is_native_na_attribute(left)||is_native_na_attribute(right))error("TypeError","boolean value of NA is ambiguous");
 if(numeric(left)&&numeric(right)){if(missing(left)||missing(right))return false;return native_scalar_key_compare(left,right)>=0;}
 error("TypeError","'>=' not supported between instances of '"+qualified_type_name(left)+"' and '"+qualified_type_name(right)+"'");
}
bool python_contains(const EventAttribute& collection,const EventAttribute& value){
 if(text_value(collection)){
  if(!text_value(value))error("TypeError","'in <string>' requires string as left operand, not "+type_name(value));
  return collection.value.find(value.value)!=std::string::npos;
 }
 if(collection.kind==AttributeKind::kBytes){if(type_name(value)!="int"&&value.kind!=AttributeKind::kBytes)error("TypeError","a bytes-like object is required, not '"+type_name(value)+"'");return false;}
 if(collection.container==AttributeContainerKind::kScalar)error("TypeError","argument of type '"+type_name(collection)+"' is not iterable");
 return member(value,admitted_values(collection));
}
bool dictionary_keys_equal(const EventAttribute& left,const EventAttribute& right){
 if(left.container!=AttributeContainerKind::kScalar||right.container!=AttributeContainerKind::kScalar){
  if(left.container!=right.container)return false;
  if(left.container==AttributeContainerKind::kTuple){
   if(left.children.size()!=right.children.size())return false;
   for(std::size_t i=0;i<left.children.size();++i)if(!dictionary_keys_equal(left.children[i],right.children[i]))return false;
   return true;
  }
  if(left.container==AttributeContainerKind::kFrozenSet){
   const auto contained=[](const auto& value,const auto& values){return std::any_of(values.begin(),values.end(),[&](const auto& other){return dictionary_keys_equal(value,other);});};
   for(const auto& value:left.children)if(!contained(value,right.children))return false;
   for(const auto& value:right.children)if(!contained(value,left.children))return false;
   return true;
  }
  return false;
 }
 if(date_value(left)||date_value(right)){
  if(!date_value(left)||!date_value(right))return false;
  if(left.is_null||right.is_null)return left.is_null&&right.is_null;
  const auto a=instant(left),b=instant(right);
  return a&&b&&a->timezone_aware==b->timezone_aware&&a->seconds==b->seconds&&a->nanoseconds==b->nanoseconds;
 }
 return root_cause_scalar_keys_equal(left,right);
}
PandasAttributesFilterOptions resolve_attributes_options(const std::vector<PandasAttributesFilterParameter>& parameters,bool relative=false){
 PandasAttributesFilterOptions result;
 auto assign=[&](const EventAttribute& value){
  if(value.key=="pm4py:param:case_id_key")result.case_id_key=value;
  else if(value.key=="pm4py:param:attribute_key")result.attribute_key=value;
  else if(value.key=="positive")result.positive=value;
  else if(value.key=="keep_nan_values")result.keep_nan_values=value;
  else if(value.key=="keep_once_per_case")result.keep_once_per_case=value;
  else if(value.key=="stream_filter_key1")result.stream_filter_key1=value;
  else if(value.key=="stream_filter_value1")result.stream_filter_value1=value;
  else if(value.key=="stream_filter_key2")result.stream_filter_key2=value;
  else if(value.key=="stream_filter_value2")result.stream_filter_value2=value;
 };
 for(const auto& p:parameters)if(p.enum_key==relative)assign(p.value);
 for(const auto& p:parameters)if(p.enum_key!=relative)assign(p.value);
 return result;
}
PandasAttributesFilterOptions resolve_attributes_options(const EventAttribute& parameters,bool relative=false){
 if(parameters.is_null||(empty_list(parameters)))return {};
 if(parameters.container==AttributeContainerKind::kContainer){std::vector<PandasAttributesFilterParameter> values;for(const auto& p:parameters.children)values.push_back({p,false});return resolve_attributes_options(values,relative);}
 detail::check_parameter_carrier(parameters,[](const std::string& type,const std::string& message){error(type,message);});
 return {};
}
} // namespace
PandasAttributesCountMap normalize_pandas_attributes_count_map(const PandasAttributesCountMap& source){
 PandasAttributesCountMap result;
 for(const auto& entry:source.entries){
  if(!root_cause_key_is_hashable(entry.first))error("TypeError","unhashable type: '"+type_name(entry.first)+"'");
  const auto found=std::find_if(result.entries.begin(),result.entries.end(),[&](const auto& previous){return dictionary_keys_equal(previous.first,entry.first);});
  if(found==result.entries.end())result.entries.push_back(entry);else found->second=entry.second;
 }
 return result;
}
EventTable filter_pandas_attributes(const EventTable& s,const EventAttribute& values,const PandasAttributesFilterOptions& o){return values_filter(s,values,o,false);}
EventTable filter_pandas_attributes_on_values(const EventTable& s,const EventAttribute& values,const PandasAttributesFilterOptions& o){return values_filter(s,values,o,false);}
EventTable filter_pandas_attributes_events(const EventTable& s,const EventAttribute& values,const PandasAttributesFilterOptions& o){return values_filter(s,values,o,true);}
EventTable filter_pandas_attributes_numeric(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const PandasAttributesFilterOptions& o){return numeric_filter(s,a,b,o,false);}
EventTable filter_pandas_attributes_numeric_events(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const PandasAttributesFilterOptions& o){return numeric_filter(s,a,b,o,true);}
EventTable filter_pandas_attributes_threshold(const EventTable& s,const EventAttribute& threshold,const PandasAttributesFilterOptions& o){
 s.validate();PandasAttributesCountMap counter;
 if(o.act_count0){
  if(const auto* map=std::get_if<PandasAttributesCountMap>(&*o.act_count0))counter=normalize_pandas_attributes_count_map(*map);
  else {const auto& v=std::get<EventAttribute>(*o.act_count0);if(v.is_null)counter=counts(s,o.activity_key);else error("AttributeError","'"+type_name(v)+"' object has no attribute 'items'");}
 }else counter=counts(s,o.activity_key);
 std::vector<EventAttribute> admitted;
 for(const auto& [key,count]:counter.entries)if(greater_equal(count,threshold)||(o.most_common_variant&&!o.most_common_variant->is_null&&python_contains(*o.most_common_variant,key)))admitted.push_back(key);
 if(admitted.size()==counter.entries.size())return s;
 const auto activity=direct_column(s,o.activity_key);std::vector<bool> selected(s.size());for(std::size_t i=0;i<s.size();++i)selected[i]=isin(s,activity,cell(s,i,activity),admitted);
 return subset(s,selected);
}
EventTable filter_pandas_attributes_top_activities(const EventTable& s,const EventAttribute& maximum,const PandasAttributesFilterOptions& o){
 s.validate();const auto counter=counts(s,o.activity_key);
 if(is_native_na_attribute(maximum))error("TypeError","boolean value of NA is ambiguous");
 if(!numeric(maximum))error("TypeError","'<' not supported between instances of '"+qualified_type_name(maximum)+"' and 'int'");
 if(native_attribute_number(maximum)<static_cast<double>(counter.entries.size())&&type_name(maximum)=="float")error("TypeError","slice indices must be integers or None or have an __index__ method");
 return s;
}
EventTable filter_pandas_attributes_relative_occurrence(const EventTable& s,const EventAttribute& stake,const PandasAttributesFilterOptions& o){
 s.validate();
 const bool once=o.keep_once_per_case&&truth(*o.keep_once_per_case);
 const bool case_denominator=!o.keep_once_per_case||once;
 // An empty frame only resolves a scalar attribute column.
 // groupby([case, key]) resolves the case column first, except on a two-row
 // frame, where pandas may take the key list itself as the grouper.
 if(once&&!empty_list(o.case_id_key)&&s.size()!=2)direct_column(s,o.case_id_key);
 if(s.size()==0){if(o.attribute_key.container==AttributeContainerKind::kScalar)direct_column(s,o.attribute_key);return s;}
 if(once&&o.attribute_key.is_null)error("TypeError","'NoneType' object is not callable");
 // A list key is one grouper array.
 if(once&&o.attribute_key.container==AttributeContainerKind::kList){
  if(o.attribute_key.children.size()!=s.size())error("ValueError","Grouper and axis must be same length");
 }
 if(empty_list(o.attribute_key))error("ValueError","No group keys passed!");
 const auto attribute=direct_column(s,o.attribute_key);
 std::optional<std::size_t> cases;std::unordered_set<std::string> all_cases;
 if(case_denominator){
  if(empty_list(o.case_id_key))error("ValueError","The truth value of a Series is ambiguous. Use a.empty, a.bool(), a.item(), a.any() or a.all().");
  cases=direct_column(s,o.case_id_key);
  for(std::size_t i=0;i<s.size();++i)if(!missing(cell(s,i,*cases)))all_cases.insert(group_key(cell(s,i,*cases)));
 }
 struct Frequency {EventAttribute value;std::size_t count=0;std::unordered_set<std::string> cases;};
 std::vector<Frequency> frequencies;std::unordered_map<std::string,std::size_t> positions;
 for(std::size_t i=0;i<s.size();++i){const auto& value=cell(s,i,attribute);if(missing(value))continue;
  // Per-case counts group by (case, value), which drops null cases.
  if(once&&missing(cell(s,i,*cases)))continue;
  const auto key=group_key(value);const auto [position,added]=positions.emplace(key,frequencies.size());if(added)frequencies.push_back({value});
  auto& f=frequencies[position->second];if(!once)++f.count;else f.cases.insert(group_key(cell(s,i,*cases)));
 }
 // stake * count: only numbers and sequences multiply by an int.
 if(stake.is_null||stake.container==AttributeContainerKind::kContainer||stake.container==AttributeContainerKind::kSet||stake.container==AttributeContainerKind::kFrozenSet||(stake.container==AttributeContainerKind::kScalar&&date_value(stake)))
  error("TypeError","unsupported operand type(s) for *: '"+qualified_type_name(stake)+"' and 'int'");
 if(is_native_na_attribute(stake))error("TypeError","boolean value of NA is ambiguous");
 if(!numeric(stake)&&!frequencies.empty())error("TypeError","'>=' not supported between instances of 'int' and '"+qualified_type_name(stake)+"'");
 const double threshold=!numeric(stake)?0.0:native_attribute_number(stake)*static_cast<double>(case_denominator?all_cases.size():s.size());
 std::vector<EventAttribute> admitted;for(const auto& f:frequencies)if(static_cast<double>(once?f.cases.size():f.count)>=threshold)admitted.push_back(f.value);
 EventAttribute values{"",""};values.container=AttributeContainerKind::kList;values.children=std::move(admitted);
 return values_filter(s,values,o,true);
}
#define MINERA_ATTRIBUTES_ONE(name,relative) \
EventTable name(const EventTable& s,const EventAttribute& v,const EventAttribute& p){return name(s,v,resolve_attributes_options(p,relative));} \
EventTable name(const EventTable& s,const EventAttribute& v,const std::vector<PandasAttributesFilterParameter>& p){return name(s,v,resolve_attributes_options(p,relative));}
MINERA_ATTRIBUTES_ONE(filter_pandas_attributes,false)
MINERA_ATTRIBUTES_ONE(filter_pandas_attributes_events,false)
MINERA_ATTRIBUTES_ONE(filter_pandas_attributes_relative_occurrence,true)
#undef MINERA_ATTRIBUTES_ONE
#define MINERA_ATTRIBUTES_TWO(name) \
EventTable name(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const EventAttribute& p){return name(s,a,b,resolve_attributes_options(p));} \
EventTable name(const EventTable& s,const EventAttribute& a,const EventAttribute& b,const std::vector<PandasAttributesFilterParameter>& p){return name(s,a,b,resolve_attributes_options(p));}
MINERA_ATTRIBUTES_TWO(filter_pandas_attributes_numeric)
MINERA_ATTRIBUTES_TWO(filter_pandas_attributes_numeric_events)
#undef MINERA_ATTRIBUTES_TWO
} // namespace minera
