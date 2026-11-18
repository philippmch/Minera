#include "minera/pandas_attributes_filter.hpp"
#include "pandas_parameter_carrier.hpp"
#include "pandas_numpy_view.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "python_scalar.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"
#include "zone_database.hpp"
#include "pandas_numpy_operand.hpp"
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
 if(v.container==AttributeContainerKind::kContainer||v.container==AttributeContainerKind::kList)
  return detail::pandas_container_repr(v,[](const EventAttribute& x){return repr(x);},[](const std::string& k){return string_key_repr(k);});
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
// Round DR: df.where(mask) for a mask of cells (alive[row][column]): a
// column with a masked cell turns int64 into float64 and bool into object,
// its masked cells null as in masked_frame; a column without one keeps its
// cells and dtype.
EventTable where_frame(const EventTable& source,const std::vector<std::vector<char>>& alive){
 const auto masked=masked_frame(source);
 EventTable result=source;
 for(std::size_t c=0;c<source.columns.size();++c){
  bool any=false;for(std::size_t i=0;i<source.size();++i)any|=!alive[i][c];
  if(!any)continue;
  if(!result.dtypes.empty())result.dtypes[c]=masked.dtypes[c];
  const bool to_float=!source.dtypes.empty()&&(source.dtypes[c]=="int64"||source.dtypes[c]=="uint64");
  for(std::size_t i=0;i<source.size();++i){
   if(!alive[i][c]){result.rows[i][c]=masked.rows[i][c];continue;}
   if(!to_float||!source.rows[i][c])continue;
   auto value=*source.rows[i][c];
   const double number=native_attribute_number(value);
   value.kind=AttributeKind::kFloat;value.typed_value=number;value.value=python::float_repr(number);
   result.rows[i][c]=std::move(value);
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
 detail::pandas_check_getitem_key(key,error);
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
 detail::pandas_check_getitem_key(key,error);
 if(key.container==AttributeContainerKind::kList)check_list_columns(source,key);
 error("KeyError",key_repr(key));
}
// set_index(key) as pandas checks it: list items that are lists are arrays,
// every other item a label; the labels missing from the columns fail first
// (KeyError naming only them), then each array whose length is not the
// frame's. Round DA: pm4py sets the index of the whole frame first and then
// of the filtered rows (`second`), so an array the length of the whole frame
// fails on the filtered one (fresh CK bank 2026112409 case 409).
std::size_t case_column(const EventTable& source,const EventAttribute& key,std::optional<std::size_t> second=std::nullopt){
 if(empty_list(key))error("ValueError","Length mismatch: Expected axis has "+std::to_string(source.size())+" elements, new values have 0 elements");
 if(const auto found=column(source,key))return *found;
 detail::pandas_check_set_index_key(key,error);
 if(key.container==AttributeContainerKind::kList&&!key.is_null&&!key.children.empty()){
  const auto array=[](const EventAttribute& c){return !c.is_null&&c.container==AttributeContainerKind::kList;};
  std::string absent;
  for(const auto& child:key.children)
   if(!array(child)&&!column(source,child))absent+=(absent.empty()?"":", ")+key_repr(child);
  if(!absent.empty())error("KeyError",string_key_repr("None of ["+absent+"] are in the columns"));
  for(const auto rows:{std::optional<std::size_t>(source.size()),second}){
   if(!rows)continue;
   for(const auto& child:key.children)
    if(array(child)&&child.children.size()!=*rows)error("ValueError","Length mismatch: Expected "+std::to_string(*rows)+" rows, received array of length "+std::to_string(child.children.size()));
  }
  error("ValueError","case keys given as arrays or several columns are outside the captured native domain");
 }
 error("KeyError",string_key_repr("None of ["+key_repr(key)+"] are in the columns"));
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
// Round CK: the shape np.asarray(values, dtype=object) gives a list of
// values (lists and tuples nest, other values are items).
std::vector<std::size_t> object_array_shape(std::vector<const EventAttribute*> level){
 std::vector<std::size_t> shape{level.size()};
 while(!level.empty()){
  std::optional<std::size_t> width;
  for(const auto* e:level){
   if(e->is_null||(e->container!=AttributeContainerKind::kList&&e->container!=AttributeContainerKind::kTuple))return shape;
   if(width&&*width!=e->children.size())return shape;
   width=e->children.size();
  }
  shape.push_back(*width);std::vector<const EventAttribute*> next;
  for(const auto* e:level)for(const auto& c:e->children)next.push_back(&c);
  level=std::move(next);
 }
 return shape;
}
// Round CK: StringArray.isin on a str column keeps, unless every value is a
// str or null, the values that are a str or for which isna(value) is true.
// isna of a list is an array shaped like the list, so its truth needs
// exactly one element (the innermost one); the kept values pass through
// np.array(..., dtype=object) again and must stay one-dimensional, a kept
// list then matching its str().
std::vector<EventAttribute> string_isin_values(const std::vector<EventAttribute>& values){
 std::vector<EventAttribute> kept;
 for(const auto& v:values){
  if(text_value(v)||(v.container==AttributeContainerKind::kScalar&&missing(v))){kept.push_back(v);continue;}
  if(v.is_null||v.container!=AttributeContainerKind::kList)continue;
  const auto shape=object_array_shape({&v});
  const auto size=std::accumulate(shape.begin(),shape.end(),std::size_t{1},std::multiplies<>());
  if(size==0)error("ValueError","The truth value of an empty array is ambiguous. Use `array.size > 0` to check that an array is not empty.");
  if(size>1)error("ValueError","The truth value of an array with more than one element is ambiguous. Use a.any() or a.all()");
  const EventAttribute* inner=&v;
  while(!inner->is_null&&(inner->container==AttributeContainerKind::kList||inner->container==AttributeContainerKind::kTuple))inner=&inner->children.front();
  if(inner->container==AttributeContainerKind::kScalar&&missing(*inner))kept.push_back(v);
 }
 std::vector<const EventAttribute*> level;for(const auto& v:kept)level.push_back(&v);
 if(const auto dims=object_array_shape(level).size();dims>1)error("ValueError","Buffer has wrong number of dimensions (expected 1, got "+std::to_string(dims)+")");
 for(auto& v:kept)if(v.container==AttributeContainerKind::kList)v=EventAttribute{"",repr(v)};
 return kept;
}
// Series.isin: in a str column a null cell matches a None or NaN value.
bool isin(const EventTable& source,std::size_t col,const EventAttribute& value,const std::vector<EventAttribute>& admitted){
 if(missing(value)&&col<source.dtypes.size()&&source.dtypes[col]=="str")
  return std::any_of(admitted.begin(),admitted.end(),[](const auto& v){const auto key=identity(v);return key=="None"||key=="NaN";});
 return member(value,admitted);
}
// Round CT: alive marks the cells a DataFrame.where kept; a selected row
// whose case cell it dropped holds a NaN case id, which Index.isin matches
// with every missing case id (an object index only with NaN).
EventTable project_cases(const EventTable& source,const EventAttribute& case_key,const std::vector<bool>& selected,bool positive,const std::vector<std::vector<char>>* alive=nullptr,bool row_subset=false){
 const auto cases=case_column(source,case_key,row_subset?std::optional<std::size_t>(static_cast<std::size_t>(std::count(selected.begin(),selected.end(),true))):std::nullopt);std::unordered_set<std::string> ids;bool nan=false;
 for(std::size_t i=0;i<source.size();++i)if(selected[i]){if(alive&&!alive->empty()&&!(*alive)[i][cases])nan=true;else ids.insert(identity(cell(source,i,cases)));}
 // Index.isin on a masked numeric index (Int64, Float64, ...) never matches
 // a <NA> label, even when the selected cases hold one (round BZ).
 const bool masked=cases<source.dtypes.size()&&detail::masked_numeric_dtype(source.dtypes[cases]);
 const bool object=source.dtypes.empty()||source.dtypes[cases]=="object";
 std::vector<bool> keep(source.size());
 for(std::size_t i=0;i<source.size();++i){
  const auto& id=cell(source,i,cases);
  const bool nan_match=nan&&(object?identity(id)=="NaN":missing(id));
  keep[i]=(!(masked&&(id.is_null||is_native_na_attribute(id)))&&(ids.contains(identity(id))||nan_match))==positive;
 }
 return subset(source,keep);
}
EventTable values_filter(const EventTable& source,const EventAttribute& values,const PandasAttributesFilterOptions& options,bool events){
 source.validate();
 if(empty_list(options.attribute_key)){
  if(!values.is_null&&values.container==AttributeContainerKind::kScalar)error("TypeError","only list-like or dict-like objects are allowed to be passed to DataFrame.isin(), you passed a '"+type_name(values)+"'");
  if(events)return masked_frame(source);
  // Round DC: df[df[[]].isin(values)] is the masked frame, whose case ids
  // are all NaN, so the null cases match as in numeric_filter (fresh pandas
  // CK 2026112609 case 455).
  const auto cases=case_column(source,options.case_id_key);std::vector<bool> nulls(source.size());
  for(std::size_t i=0;i<source.size();++i)nulls[i]=missing(cell(source,i,cases));
  return project_cases(source,options.case_id_key,nulls,truth(options.positive));
 }
 // Round DR: a list key filters df[list].isin(values), a frame: each listed
 // cell holding a value (or NaN, keeping NaN values) stays, every other
 // cell is masked (DataFrame.where); per case the masked frame's case ids
 // are read as in numeric_filter (probe dr/p1).
 if(!options.attribute_key.is_null&&options.attribute_key.container==AttributeContainerKind::kList){
  check_list_columns(source,options.attribute_key);
  // apply_events passes None on (the case filters read it as []).
  if((values.is_null&&events)||(!values.is_null&&values.container==AttributeContainerKind::kScalar))error("TypeError","only list-like or dict-like objects are allowed to be passed to DataFrame.isin(), you passed a '"+type_name(values)+"'");
  const auto admitted=values.is_null?std::vector<EventAttribute>{}:values.children;
  const bool positive=truth(options.positive),keep_nan=truth(options.keep_nan_values);
  std::vector<std::vector<char>> alive(source.size(),std::vector<char>(source.columns.size(),0));
  std::vector<std::size_t> columns;
  for(const auto& item:options.attribute_key.children)columns.push_back(*column(source,item));
  for(const auto c:columns){
   // A dict maps a column name to that column's values (Series.isin, which
   // takes only a list-like); a column it does not name matches nothing.
   std::vector<EventAttribute> listed;
   if(values.container==AttributeContainerKind::kContainer){
    for(const auto& entry:values.children)if(entry.key==source.columns[c]){listed=admitted_values(entry);break;}
   } else listed=admitted;
   if(c<source.dtypes.size()&&source.dtypes[c]=="str")listed=string_isin_values(listed);
   for(std::size_t i=0;i<source.size();++i){
    const auto& v=cell(source,i,c);
    const bool match=isin(source,c,v,listed)||(keep_nan&&missing(v));
    alive[i][c]=(events&&!positive)?!match:match;
   }
  }
  // df[mask] aligns the mask's columns to the frame's.
  for(std::size_t j=0;j<columns.size();++j)for(std::size_t k=0;k<j;++k)
   if(columns[j]==columns[k])error("ValueError","cannot reindex on an axis with duplicate labels");
  if(events)return source.size()==0?source:where_frame(source,alive);
  std::vector<bool> selected(source.size(),true);
  return project_cases(source,options.case_id_key,selected,positive,&alive);
 }
 const auto attribute=direct_column(source,options.attribute_key);
 auto admitted=admitted_values(values,!events);
 if(attribute<source.dtypes.size()&&source.dtypes[attribute]=="str"&&values.container!=AttributeContainerKind::kContainer)admitted=string_isin_values(admitted);
 const bool positive=truth(options.positive),keep_nan=truth(options.keep_nan_values);
 std::vector<bool> selected(source.size());
 for(std::size_t i=0;i<source.size();++i){const auto& v=cell(source,i,attribute);const bool match=isin(source,attribute,v,admitted);selected[i]=events?((match||(keep_nan&&missing(v)))==positive):(match||(keep_nan&&missing(v)));}
 return events?subset(source,selected):project_cases(source,options.case_id_key,selected,positive,nullptr,true);
}
EventAttribute float32_bound(const EventAttribute& bound){
 if(!numeric(bound)||missing(bound))return bound;
 EventAttribute result=bound;result.kind=AttributeKind::kFloat;result.typed_value=static_cast<double>(static_cast<float>(native_attribute_number(bound)));return result;
}
// Round CK: a str or datetime64 column compares a dict as an array of its
// keys, which numpy then holds as one 0-d object; other columns compare it
// as a scalar. Round DB: a set or frozenset compares the same way over its
// items, as in the LTL filters (probe set-pdtblck-2).
void dict_operand(const EventAttribute& operand,std::size_t rows,const std::string& dtype){
 const auto kind=operand.container;
 if((kind!=AttributeContainerKind::kContainer&&kind!=AttributeContainerKind::kSet&&kind!=AttributeContainerKind::kFrozenSet)||operand.is_null)return;
 const auto n=operand.children.size();
 if(dtype=="str"){
  if(n!=rows)error("ValueError","Lengths of operands do not match: "+std::to_string(rows)+" != "+std::to_string(n));
  error("IndexError","too many indices for array: array is 0-dimensional, but 1 were indexed");
 }
 if(dtype.starts_with("datetime64")){
  if(n!=rows)error("ValueError","Lengths must match");
  error("ValueError","Buffer has wrong number of dimensions (expected 1, got 0)");
 }
}
// op is one of ">=", "<=", "<", ">" (value op bound).
// Per-row bounds. A tuple is an array operand: a str column needs equal
// lengths, other columns broadcast one item (or one row) with numpy.
// Round DP: a str bound against a datetime64 column is the Timestamp pandas
// parses from it (probe dp/n2): the NaT texts match no row, an ISO-8601 text
// compares as its instant (a naive one in an aware column's zone, which
// refuses a wall time the zone skips or repeats; a naive column refuses an
// aware text). Texts outside ISO-8601 go through pandas' dateutil fallback,
// which Minera does not reproduce: they throw std::invalid_argument.
std::string exact_instant_text(const ParsedExactTimestamp& at){
 const std::int64_t days=at.seconds>=0?at.seconds/86400:-((-at.seconds+86399)/86400);
 const std::int64_t rest=at.seconds-days*86400;
 // civil_from_days (H. Hinnant)
 const std::int64_t z=days+719468;const std::int64_t era=(z>=0?z:z-146096)/146097;const std::int64_t doe=z-era*146097;
 const std::int64_t yoe=(doe-doe/1460+doe/36524-doe/146096)/365;std::int64_t y=yoe+era*400;const std::int64_t doy=doe-(365*yoe+yoe/4-yoe/100);
 const std::int64_t mp=(5*doy+2)/153;const std::int64_t d=doy-(153*mp+2)/5+1;const std::int64_t m=mp<10?mp+3:mp-9;if(m<=2)++y;
 char buffer[64];
 std::snprintf(buffer,sizeof buffer,"%04lld-%02lld-%02lldT%02lld:%02lld:%02lld.%09u",static_cast<long long>(y),static_cast<long long>(m),static_cast<long long>(d),
               static_cast<long long>(rest/3600),static_cast<long long>(rest/60%60),static_cast<long long>(rest%60),static_cast<unsigned>(at.nanoseconds));
 return std::string(buffer)+(at.timezone_aware?"+00:00":"");
}
// The texts pandas reads as a Timestamp in the ISO shapes it accepts
// (observed on pandas 3.0.5): a year, a year and month, a date with one- or
// two-digit month and day or as eight digits, then a time of an hour, hours
// and minutes (with or without a colon), seconds and a fraction (nine digits
// kept), then Z, an offset or UTC (a space allowed before). A field out of
// range fails (no fallback rescues it); nullopt: not one of these shapes.
// Round DQ: nano marks a fraction of more than six digits, which pandas
// parses at nanosecond resolution (probe dq/n4).
struct TextTimestamp{bool refused=false;std::optional<ParsedExactTimestamp> at;bool nano=false;};
std::optional<TextTimestamp> iso_text_timestamp(std::string_view s){
 std::size_t p=0;
 const auto digits=[&](std::size_t lo,std::size_t hi,int& out)->bool{
  std::size_t n=0;out=0;
  while(p<s.size()&&n<hi&&std::isdigit(static_cast<unsigned char>(s[p]))){out=out*10+(s[p]-'0');++p;++n;}
  return n>=lo;
 };
 int year=0,month=1,day=1,hour=0,minute=0,second=0;std::uint32_t nanos=0;bool nano=false;
 const auto start=p;
 if(!digits(4,4,year))return std::nullopt;
 bool range=true;
 if(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p]))){
  // eight digits
  if(!digits(2,2,month)||!digits(2,2,day))return std::nullopt;
  if(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p])))return std::nullopt;
 }else if(p<s.size()&&s[p]=='-'){
  ++p;if(!digits(1,2,month))return std::nullopt;
  if(p<s.size()&&s[p]=='-'){++p;if(!digits(1,2,day))return std::nullopt;}
  else if(p!=s.size())return std::nullopt;
 }else if(p!=s.size())return std::nullopt;
 (void)start;
 static const int month_days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 const bool leap=(year%4==0&&year%100!=0)||year%400==0;
 if(month<1||month>12)range=false;
 else if(day<1||day>month_days[month-1]+(month==2&&leap?1:0))range=false;
 bool aware=false;int offset=0;
 if(p<s.size()){
  if(s[p]!='T'&&s[p]!=' ')return std::nullopt;
  ++p;
  if(!digits(2,2,hour))return std::nullopt;
  if(p<s.size()&&s[p]==':'){++p;if(!digits(2,2,minute))return std::nullopt;
   if(p<s.size()&&s[p]==':'){++p;if(!digits(2,2,second))return std::nullopt;
    if(p<s.size()&&s[p]=='.'){++p;std::size_t n=0;while(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p]))){if(n<9)nanos=nanos*10+static_cast<std::uint32_t>(s[p]-'0');++n;++p;}
     nano=n>6;for(;n<9;++n)nanos*=10;}
   }
  }else if(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p]))){if(!digits(2,2,minute))return std::nullopt;}
  if(hour>23||minute>59||second>59)range=false;
  if(p<s.size()&&s[p]==' ')++p;
  if(p<s.size()){
   if(s.substr(p)=="Z"||s.substr(p)=="UTC"){aware=true;p=s.size();}
   else if(s[p]=='+'||s[p]=='-'){
    const int sign=s[p]=='-'?-1:1;++p;int oh=0,om=0;
    if(!digits(2,2,oh))return std::nullopt;
    if(p<s.size()&&s[p]==':')++p;
    if(p<s.size()&&!digits(2,2,om))return std::nullopt;
    if(p!=s.size()||oh>23||om>59)return std::nullopt;
    aware=true;offset=sign*(oh*60+om);
   }else return std::nullopt;
  }
 }
 if(!range)return TextTimestamp{true,std::nullopt};
 char buffer[48];
 std::snprintf(buffer,sizeof buffer,"%04d-%02d-%02dT%02d:%02d:%02d",year,month,day,hour,minute,second);
 auto parsed=parse_timestamp_exact(buffer);
 if(!parsed)return std::nullopt;
 parsed->nanoseconds=nanos;
 if(aware){parsed->seconds-=static_cast<std::int64_t>(offset)*60;parsed->timezone_aware=true;parsed->offset_minutes=offset;}
 return TextTimestamp{false,parsed,nano};
}
// Round DQ: the int64 nanosecond range of pandas (Timestamp.min and max,
// 1677-09-21 00:12:43.145224193 to 2262-04-11 23:47:16.854775807 UTC).
bool before_nanosecond_range(const ParsedExactTimestamp& at){return at.seconds<-9223372037||(at.seconds==-9223372037&&at.nanoseconds<145224193);}
bool after_nanosecond_range(const ParsedExactTimestamp& at){return at.seconds>9223372036||(at.seconds==9223372036&&at.nanoseconds>854775807);}
// Round DQ: a wall clock within a day of that range's ends (where pandas'
// int64 localization of a nanosecond value wraps or refuses unevenly in
// zones with transitions; probe dq/edge.py over all 599 zones).
bool near_nanosecond_edge(const ParsedExactTimestamp& at){return at.seconds<-9223372037+86400||at.seconds>9223372036-86400;}
// Round DQ: the year past 9999 of an instant (10000-01-01T00:00:00Z on).
bool past_year_9999(const ParsedExactTimestamp& at){return at.seconds>=253402300800;}
// A zone whose offset changes over time (not a fixed Etc zone): its offset
// before the first transition differs from a winter or summer one of 2021.
bool zone_with_transitions(std::string_view zone){
 try{
  const auto early=::minera::zone_database::info_at_utc(zone,-9223372037).offset_seconds;
  return early!=::minera::zone_database::info_at_utc(zone,1609459200).offset_seconds||early!=::minera::zone_database::info_at_utc(zone,1625097600).offset_seconds;
 }catch(const std::runtime_error&){return false;}
}
// Round DQ: a zone still keeping daylight saving time in 9999 (its January
// and July offsets differ), which pandas localizes through Python's datetime
// past its last listed transition (probe over all 599 zones).
bool zone_with_future_dst(std::string_view zone){
 try{
  return ::minera::zone_database::info_at_utc(zone,253371974400).offset_seconds!=::minera::zone_database::info_at_utc(zone,253387612800).offset_seconds;
 }catch(const std::runtime_error&){return false;}
}
// Pandas refuses at once a text that reads as a number below 1000 (a float
// of digits, then an optional fraction and exponent, spaces around), unless
// it starts with "0", and the one letters a, m, p and t.
bool unlikely_datetime_text(std::string_view s){
 if(s=="a"||s=="A"||s=="m"||s=="M"||s=="p"||s=="P"||s=="t"||s=="T")return true;
 if(s.empty()||s.front()=='0')return false;
 std::size_t p=0;while(p<s.size()&&s[p]==' ')++p;
 const auto begin=p;
 if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;
 const auto int_start=p;while(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p])))++p;
 if(p==int_start)return false;
 // Four digits or more without a fraction or exponent read as a year first
 // (probe dp/n3: "-1172" is the year -1172).
 const bool year=p-int_start>=4;
 if(p<s.size()&&s[p]=='.'){++p;while(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p])))++p;}
 else if(year&&(p==s.size()||(s[p]!='e'&&s[p]!='E')))return false;
 if(p<s.size()&&(s[p]=='e'||s[p]=='E')){
  ++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;
  const auto e=p;while(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p])))++p;
  if(p==e)return false;
 }
 const auto end=p;while(p<s.size()&&s[p]==' ')++p;
 if(p!=s.size())return false;
 return std::strtod(std::string(s.substr(begin,end-begin)).c_str(),nullptr)<1000;
}
// Round DT (probes dt t3, dt/nb/dtp3.py): a text without a digit names no
// date unless one of its words is a month (pandas reads "now" and "today"
// itself); "b", "x y", "Mon" or "\u00e9" are invalid comparisons.
bool dateless_text(std::string_view s){
 if(s=="now"||s=="today")return false;
 if(std::any_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';}))return false;
 static const std::unordered_set<std::string> months={"jan","feb","mar","apr","may","jun","jul","aug","sep","sept","oct","nov","dec",
  "january","february","march","april","june","july","august","september","october","november","december"};
 std::string word;
 const auto month=[&]{const bool hit=months.contains(word);word.clear();return hit;};
 for(const char c:s){
  if((c>='a'&&c<='z')||(c>='A'&&c<='Z'))word+=static_cast<char>(c|0x20);
  else if(!word.empty()&&month())return false;
 }
 return !(!word.empty()&&month());
}
EventAttribute datetime_text_bound(const EventAttribute& bound,const std::string& dtype){
 const std::string_view text=bound.value;
 if(text.empty()||text=="NaT"||text=="nat"||text=="NAT"||text=="NaN"||text=="nan"||text=="NAN"){
  EventAttribute nat{bound.key,"NaT",AttributeKind::kDate,true};return nat;
 }
 const auto refuse=[&]{error("TypeError","Invalid comparison between dtype="+dtype+" and str");};
 if(unlikely_datetime_text(text)||dateless_text(text))refuse();
 auto trimmed=text;
 while(!trimmed.empty()&&(trimmed.front()==' '||trimmed.front()=='\t'))trimmed.remove_prefix(1);
 while(!trimmed.empty()&&(trimmed.back()==' '||trimmed.back()=='\t'))trimmed.remove_suffix(1);
 const auto read=iso_text_timestamp(trimmed);
 if(!read)throw std::invalid_argument("a date string outside the ISO shapes against a pandas datetime64 column (pandas' dateutil fallback) is outside the native domain");
 if(read->refused)refuse();
 auto parsed=read->at;
 // Round DQ: a nanosecond text must lie in the int64 nanosecond range (its
 // wall clock, or its instant when it names an offset).
 if(read->nano&&(before_nanosecond_range(*parsed)||after_nanosecond_range(*parsed)))refuse();
 const auto comma=dtype.find(", ");
 const bool aware=comma!=std::string::npos&&dtype.ends_with("]");
 const auto zone=aware?dtype.substr(comma+2,dtype.size()-comma-3):std::string();
 if(aware&&!parsed->timezone_aware){
  const auto nanos=parsed->nanoseconds;
  const bool transitions=zone_with_transitions(zone);
  if(read->nano&&transitions&&near_nanosecond_edge(*parsed))
   throw std::invalid_argument("a nanosecond date string within a day of pandas' int64 range ends against a datetime64 column in a zone with transitions (pandas' localization wraps there) is outside the native domain");
  parsed=localize_timestamp_wall_time(*parsed,zone);
  if(!parsed)refuse();
  parsed->nanoseconds=nanos;
  // Round DQ: past its last transition a zone that keeps daylight saving
  // time localizes through Python's datetime, which ends with 9999 (probe
  // dq/n5: 9999-12-31 23:59:59.999999 in America/New_York).
  if(past_year_9999(*parsed)&&zone_with_future_dst(zone))
   error("NotImplementedError","Localizing Timestamps which are outside the range of Python's standard library datetime is not supported (year 10000 is out of range).");
  // Round DQ: pandas localizes with the zone's transitions from
  // Timestamp.min on, so in a zone with transitions a wall time whose
  // instant falls before it is nonexistent (probe dq/n4: 1600-01-01 in
  // Europe/Berlin, 1677-09-21 01:06:11 there).
  if(before_nanosecond_range(*parsed)&&transitions)refuse();
  // Round DQ: a nanosecond value's instant must lie in the int64 range, in
  // a fixed zone too (probe dq/n5).
  if(read->nano&&(before_nanosecond_range(*parsed)||after_nanosecond_range(*parsed)))refuse();
 }else if(aware&&read->nano){
  // Round DQ: an aware nanosecond value converts to the column's zone, whose
  // wall clock must lie in the int64 range too (probe dq/n5: ...Z against
  // Europe/London, whose LMT is behind UTC).
  ParsedExactTimestamp local=*parsed;
  if(zone.size()==9&&zone.starts_with("UTC")&&(zone[3]=='+'||zone[3]=='-')&&zone[6]==':'){
   const int minutes=((zone[4]-'0')*10+(zone[5]-'0'))*60+(zone[7]-'0')*10+(zone[8]-'0');
   local.seconds+=std::int64_t{zone[3]=='-'?-minutes:minutes}*60;
  }else if(zone!="UTC"){
   try{local.seconds+=::minera::zone_database::info_at_utc(zone,parsed->seconds).offset_seconds;}catch(const std::runtime_error&){}
  }
  if(before_nanosecond_range(local)||after_nanosecond_range(local))refuse();
 }
 if(!aware&&parsed->timezone_aware)refuse();
 return EventAttribute{bound.key,exact_instant_text(*parsed),AttributeKind::kDate};
}
// Round DT (probe dt t1; black-box fuzz dt/nb/strfz.py, 19,000 calls): a
// "str" column checks a tuple's length first, then makes it a numpy array
// (whose leaves turn into str when one is a str and none is another
// object), keeps the rows that are not missing, broadcasts them with the
// array's rows and compares each pair as Python does; a result of more than
// one dimension cannot be assigned to the rows.
std::vector<bool> string_tuple_mask(const EventTable& source,std::size_t attribute,const EventAttribute& bound,const std::string& op){
 const auto n=source.size(),k=bound.children.size();
 if(k!=n)error("ValueError","Lengths of operands do not match: "+std::to_string(n)+" != "+std::to_string(k));
 const auto array=detail::numpy_tuple_array(bound,error);
 enum class Leaves{kObject,kStr,kFloat,kInt,kBool};
 const auto number=[](const EventAttribute* e){return !e->is_null&&e->container==AttributeContainerKind::kScalar&&!is_native_na_attribute(*e)&&!date_value(*e)&&numeric(*e);};
 const auto boolean=[](const EventAttribute* e){return e->kind==AttributeKind::kBoolean||std::holds_alternative<bool>(e->typed_value);};
 const auto integer=[&](const EventAttribute* e){return !boolean(e)&&(e->kind==AttributeKind::kInt||std::holds_alternative<std::int64_t>(e->typed_value)||std::holds_alternative<std::uint64_t>(e->typed_value));};
 Leaves leaves=Leaves::kFloat;
 const auto& all=array.leaves;
 if(std::any_of(all.begin(),all.end(),[&](const EventAttribute* e){return !number(e)&&!text_value(*e);}))leaves=Leaves::kObject;
 else if(std::any_of(all.begin(),all.end(),[](const EventAttribute* e){return text_value(*e);}))leaves=Leaves::kStr;
 else if(std::any_of(all.begin(),all.end(),[&](const EventAttribute* e){return !integer(e)&&!boolean(e);}))leaves=Leaves::kFloat;
 else if(std::any_of(all.begin(),all.end(),integer))leaves=Leaves::kInt;
 else if(!all.empty())leaves=Leaves::kBool;
 const auto as_text=[&](const EventAttribute& e)->std::string{
  if(text_value(e))return e.value;
  if(boolean(&e))return native_attribute_number(e)!=0?"True":"False";
  if(integer(&e))return e.value;
  return python::float_repr(native_attribute_number(e));
 };
 const auto compare=[&](const EventAttribute& value,const EventAttribute& item)->bool{
  std::string other;
  if(leaves==Leaves::kStr)other=as_text(item);
  else if(leaves==Leaves::kObject&&text_value(item))other=item.value;
  else{
   const std::string name=leaves==Leaves::kFloat?"float":leaves==Leaves::kInt?"int":leaves==Leaves::kBool?"bool":item.is_null?std::string("NoneType"):qualified_type_name(item);
   error("TypeError","'"+op+"' not supported between instances of 'str' and '"+name+"'");
  }
  const int order=value.value<other?-1:other<value.value?1:0;
  return op==">="?order>=0:op=="<="?order<=0:op=="<"?order<0:order>0;
 };
 std::vector<std::size_t> valid;
 for(std::size_t i=0;i<n;++i)if(!missing(cell(source,i,attribute)))valid.push_back(i);
 const auto v=valid.size();
 auto shape=array.shape;shape.front()=v;
 const auto last=shape.back();
 if(last!=v&&last!=1&&v!=1)
  error("ValueError","operands could not be broadcast together with shapes ("+std::to_string(v)+",) "+detail::numpy_broadcast_shape(shape)+" ");
 const auto per_row=n?all.size()/n:0;
 std::vector<const EventAttribute*> flat;
 for(const auto i:valid)for(std::size_t j=0;j<per_row;++j)flat.push_back(all[i*per_row+j]);
 const auto width=last==1?v:last;
 const auto outer=last?flat.size()/last:0;
 std::vector<bool> result;
 for(std::size_t o=0;o<outer;++o)
  for(std::size_t j=0;j<width;++j)result.push_back(compare(cell(source,valid[v==1?0:j],attribute),*flat[o*last+(last==1?0:j)]));
 if(shape.size()>1)error("TypeError","NumPy boolean array indexing assignment requires a 0 or 1-dimensional input, input has "+std::to_string(shape.size())+" dimensions");
 std::vector<bool> out(n);
 for(std::size_t j=0;j<v;++j)out[valid[j]]=result[j];
 return out;
}
bool numpy_number_dtype(const std::string& dtype){return dtype=="bool"||dtype.starts_with("int")||dtype.starts_with("uint")||dtype.starts_with("float");}
// pandas' masked number dtypes (Int8 .. UInt64, Float32, Float64, boolean)
// and the numpy dtype of their data.
bool masked_number_dtype(const std::string& dtype){return dtype=="boolean"||dtype.starts_with("Int")||dtype.starts_with("UInt")||dtype.starts_with("Float");}
std::string masked_numpy_dtype(const std::string& dtype){
 if(dtype=="boolean")return "bool";
 std::string out=dtype;for(auto& c:out)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return out;
}
// numpy's dtype class name: int64 -> Int64DType, uint8 -> UInt8DType.
std::string numpy_dtype_class(const std::string& dtype){
 if(dtype=="bool")return "BoolDType";
 if(dtype.starts_with("uint"))return "UInt"+dtype.substr(4)+"DType";
 std::string out=dtype;out[0]=static_cast<char>(out[0]-'a'+'A');return out+"DType";
}
std::vector<EventAttribute> row_bounds(const EventAttribute& bound,std::size_t rows,const std::string& dtype){
 if(text_value(bound)&&bound.container==AttributeContainerKind::kScalar&&!bound.is_null&&dtype.starts_with("datetime64"))
  return std::vector<EventAttribute>(rows,datetime_text_bound(bound,dtype));
 if(text_value(bound)&&dtype!="object"&&dtype!="str")error("TypeError","Invalid comparison between dtype="+dtype+" and str");
 // Round DO: a datetime64 column refuses None, a number and a datetime of
 // the other time-zone kind (naive against aware or the reverse) as a
 // whole, before any row compares (probe n1).
 if(dtype.starts_with("datetime64")&&bound.container==AttributeContainerKind::kScalar&&!is_native_na_attribute(bound)){
  const bool aware=dtype.find(',')!=std::string::npos;
  bool refused=(bound.is_null&&!date_value(bound))||(!bound.is_null&&numeric(bound));
  if(!bound.is_null&&date_value(bound))if(const auto at=instant(bound))refused=at->timezone_aware!=aware;
  if(refused)error("TypeError","Invalid comparison between dtype="+dtype+" and "+type_name(bound));
 }
 dict_operand(bound,rows,dtype);
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
// Round DU (probe du v1b): a Python float scalar is weak, so a float32
// column (numpy or masked) rounds it to float32 first; an array operand (a
// tuple or list) is float64 and promotes the column instead. The dtype an
// item of an array compares under.
std::string array_item_dtype(const std::string& dtype){return dtype=="float32"?"float64":dtype=="Float32"?"Float64":dtype;}
bool comparison(const EventAttribute& value,const EventAttribute& bound,const std::string& op,const std::string& dtype){
 // Round CK: a NaN cell of a float column still compares with a dict, list
 // or set (Python's own comparison error).
 // Round DO: and with a datetime (fresh pandas CK 2026120821 case 166,
 // probe do/pn1.py).
 if(missing(value)&&dtype.starts_with("float")&&!bound.is_null&&
    (bound.container!=AttributeContainerKind::kScalar||(!missing(bound)&&date_value(bound))))
  error("TypeError","'"+op+"' not supported between instances of 'float' and '"+qualified_type_name(bound)+"'");
 if(missing(value)||missing(bound))return false;
 int order=0;
 if(text_value(value)&&text_value(bound))order=value.value<bound.value?-1:bound.value<value.value?1:0;
 else if(date_value(value)&&date_value(bound)){
  const auto a=instant(value),b=instant(bound);if(!a||!b)return false;
  order=a->seconds==b->seconds?(a->nanoseconds<b->nanoseconds?-1:a->nanoseconds>b->nanoseconds?1:0):(a->seconds<b->seconds?-1:1);
 }else if(numeric(value)&&numeric(bound))order=native_scalar_key_compare(value,dtype=="float32"||dtype=="Float32"?float32_bound(bound):bound);
 else error("TypeError","'"+op+"' not supported between instances of '"+qualified_type_name(value)+"' and '"+qualified_type_name(bound)+"'");
 return op==">="?order>=0:op=="<="?order<=0:op=="<"?order<0:order>0;
}
// Round DU (probe du v1b; black-box probes du/u/mk3.py, mk4.py, mk5.py): a
// list operand of a number column (numpy or masked; its length already
// checked) is the array pandas infers from it. Numbers with NaN or None
// (None only without a bool) are numbers, where NaN and None compare False;
// str with None or NaN is a str array, which a numpy column compares
// reflected (the str's own error) and a masked one as its first str; naive
// datetimes, or aware ones of one offset, with None or NaN are a datetime
// array, which a numpy column refuses as a whole and a masked one has no
// loop for (naive) or compares as Timestamps (aware); anything else (only
// None, a container, a mix) is an object array compared row by row, the
// first item that is no number raising Python's TypeError.
std::vector<bool> list_operand_mask(const EventTable& source,std::size_t attribute,const EventAttribute& list,const std::string& op,const std::string& dtype){
 const bool masked=masked_number_dtype(dtype);
 const auto numpy=masked?masked_numpy_dtype(dtype):dtype;
 const std::string python=numpy.starts_with("float")?"float":numpy=="bool"?"bool":"int";
 bool none=false,boolean=false,number=false,text=false,date=false,other=false,naive=false,aware=false,offsets=false;
 std::optional<std::int32_t> offset;
 for(const auto& item:list.children){
  if(is_native_na_attribute(item)||item.container!=AttributeContainerKind::kScalar){other=true;continue;}
  if(item.is_null){if(date_value(item))date=true;else none=true;continue;}
  if(date_value(item)){
   date=true;const auto at=instant(item);
   if(!at){other=true;continue;}
   if(!at->timezone_aware)naive=true;
   else{aware=true;if(offset&&*offset!=at->offset_minutes)offsets=true;offset=at->offset_minutes;}
   continue;
  }
  if(text_value(item)){text=true;continue;}
  if(!numeric(item)){other=true;continue;}
  if(item.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(item.typed_value))boolean=true;
  else if(!missing(item))number=true;
 }
 enum{kNumber,kString,kDatetime,kObject} kind=kObject;
 if(other||(naive&&aware)||offsets)kind=kObject;
 else if(text)kind=boolean||number||date?kObject:kString;
 else if(date)kind=boolean||number?kObject:kDatetime;
 else if(boolean&&none)kind=kObject;
 else if(boolean||number||std::any_of(list.children.begin(),list.children.end(),[](const EventAttribute& e){return !e.is_null;}))kind=kNumber;
 const auto n=source.size();
 std::vector<bool> out(n);
 if(kind==kString){
  if(!masked){
   const std::string reflected=op==">="?"<=":op=="<="?">=":op=="<"?">":"<";
   error("TypeError","'"+reflected+"' not supported between instances of 'str' and '"+python+"'");
  }
  error("TypeError","'"+op+"' not supported between instances of '"+python+"' and 'str'");
 }
 if(kind==kDatetime){
  if(!masked){
   std::string zone;
   if(aware){
    const auto minutes=*offset;
    if(minutes==0)zone=", UTC";
    else{char buffer[32];std::snprintf(buffer,sizeof buffer,", UTC%c%02d:%02d",minutes<0?'-':'+',std::abs(minutes)/60,std::abs(minutes)%60);zone=buffer;}
   }
   error("TypeError","Invalid comparison between dtype=datetime64[us"+zone+"] and ndarray");
  }
  if(aware)error("TypeError","'"+op+"' not supported between instances of '"+python+"' and 'Timestamp'");
  error("UFuncTypeError","ufunc '"+detail::numpy_ufunc_name(op)+"' did not contain a loop with signature matching types (<class 'numpy.dtypes."+numpy_dtype_class(numpy)+"'>, <class 'numpy.dtypes.DateTime64DType'>) -> None");
 }
 for(std::size_t i=0;i<n;++i){
  const auto& item=list.children[i];
  if(kind==kObject&&(item.is_null||item.container!=AttributeContainerKind::kScalar||!numeric(item)||date_value(item)||is_native_na_attribute(item)))
   error("TypeError","'"+op+"' not supported between instances of '"+python+"' and '"+(item.is_null&&!date_value(item)?std::string("NoneType"):qualified_type_name(item))+"'");
  out[i]=comparison(cell(source,i,attribute),item,op,array_item_dtype(dtype));
 }
 return out;
}
// Round DT (probe dt t1; black-box probes dt/nb/dtp.py, dtp2.py): a
// datetime64 column checks a tuple's length, refuses an array of more than
// one dimension, and then reads its items as pandas infers them (None, NaN
// and NaT skipped): aware datetimes, or date strings, compare as instants;
// numbers, booleans, naive datetimes or only NaN/NaT are an invalid
// comparison; any other mix (and only None) compares row by row as Python
// objects, where a NaT row or a missing item is False.
std::vector<bool> datetime_tuple_mask(const EventTable& source,std::size_t attribute,const EventAttribute& bound,const std::string& op,const std::string& dtype){
 const auto n=source.size(),k=bound.children.size();
 if(k!=n)error("ValueError","Lengths must match");
 const auto depth=detail::numpy_object_depth(bound);
 if(depth>1)error("ValueError","Buffer has wrong number of dimensions (expected 1, got "+std::to_string(depth)+")");
 const auto invalid=[&]{error("TypeError","Invalid comparison between dtype="+dtype+" and tuple");};
 const auto nan=[](const EventAttribute& e){return !e.is_null&&e.container==AttributeContainerKind::kScalar&&!date_value(e)&&numeric(e)&&!(e.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(e.typed_value))&&std::isnan(native_attribute_number(e));};
 const auto boolean=[](const EventAttribute& e){return e.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(e.typed_value);};
 const auto integer=[&](const EventAttribute& e){return !boolean(e)&&(e.kind==AttributeKind::kInt||std::holds_alternative<std::int64_t>(e.typed_value)||std::holds_alternative<std::uint64_t>(e.typed_value));};
 bool none_only=!bound.children.empty(),structured=false,has_int=false,has_float=false,has_bool=false,has_str=false,has_aware=false,has_naive=false,has_other=false;
 for(const auto& e:bound.children){
  if(e.is_null)continue;
  none_only=false;
  if(e.container!=AttributeContainerKind::kScalar){structured=true;continue;}
  if(nan(e)||(date_value(e)&&missing(e)))continue;
  if(is_native_na_attribute(e)){has_other=true;continue;}
  if(date_value(e)){const auto at=instant(e);if(at&&at->timezone_aware)has_aware=true;else has_naive=true;continue;}
  if(text_value(e)){has_str=true;continue;}
  if(boolean(e)){has_bool=true;continue;}
  if(integer(e)){has_int=true;continue;}
  if(numeric(e)){has_float=true;continue;}
  has_other=true;
 }
 std::vector<bool> out(n);
 const int kinds=int(has_int||has_float)+int(has_bool)+int(has_str)+int(has_aware||has_naive)+int(has_other);
 const bool elementwise=none_only||structured||kinds>1||has_other||(has_int&&has_bool)||(has_aware&&has_naive);
 if(elementwise){
  for(std::size_t i=0;i<n;++i){
   const auto& value=cell(source,i,attribute);const auto& item=bound.children[i];
   if(missing(value)||item.is_null||nan(item)||(date_value(item)&&missing(item)))continue;
   if(date_value(item)){
    const auto at=instant(item);
    if(!at||!at->timezone_aware)error("TypeError","can't compare offset-naive and offset-aware datetimes");
    out[i]=comparison(value,item,op,dtype);continue;
   }
   const std::string name=item.container==AttributeContainerKind::kContainer?"dict":item.container==AttributeContainerKind::kList?"list":item.container==AttributeContainerKind::kTuple?"tuple":item.container==AttributeContainerKind::kSet?"set":item.container==AttributeContainerKind::kFrozenSet?"frozenset":qualified_type_name(item);
   error("TypeError","'"+op+"' not supported between instances of 'Timestamp' and '"+name+"'");
  }
  return out;
 }
 if(has_aware&&!has_naive){
  for(std::size_t i=0;i<n;++i)out[i]=comparison(cell(source,i,attribute),bound.children[i],op,dtype);
  return out;
 }
 if(has_str){
  std::vector<EventAttribute> items;
  for(const auto& e:bound.children){
   if(!text_value(e)){items.push_back(EventAttribute{"","NaT",AttributeKind::kDate,true});continue;}
   try{items.push_back(datetime_text_bound(e,dtype));}catch(const PandasAttributesFilterError&){invalid();}
  }
  for(std::size_t i=0;i<n;++i)out[i]=comparison(cell(source,i,attribute),items[i],op,dtype);
  return out;
 }
 invalid();
}
// The stream filters of apply_numeric: each compares the rows that remain
// (`alive` holds the cells a list key's DataFrame.where left visible).
void stream_filters(const EventTable& source,const PandasAttributesFilterOptions& options,std::vector<bool>& selected,std::vector<std::vector<char>>& alive){
  for(const auto& control:{std::pair{&options.stream_filter_key1,&options.stream_filter_value1},std::pair{&options.stream_filter_key2,&options.stream_filter_value2}}){
   if(!*control.first||(*control.first)->is_null)continue;
   static const EventAttribute none{"","",AttributeKind::kString,true};
   const auto& value=*control.second?**control.second:none;
   std::vector<std::size_t> remaining;for(std::size_t i=0;i<source.size();++i)if(selected[i])remaining.push_back(i);
   const auto visible=[&](std::size_t row,std::size_t c){return alive.empty()||alive[row][c];};
   if((*control.first)->container==AttributeContainerKind::kList){
    const auto& key=**control.first;check_list_columns(source,key);
    std::vector<std::size_t> listed;for(const auto& item:key.children)listed.push_back(*column(source,item));
    // A list-like value aligns to the listed columns: a list or tuple by
    // position, a dict by its keys; a set has no order.
    std::vector<const EventAttribute*> operands(listed.size(),&value);
    const bool sequence=value.container==AttributeContainerKind::kList||value.container==AttributeContainerKind::kTuple;
    const bool set=value.container==AttributeContainerKind::kSet||value.container==AttributeContainerKind::kFrozenSet;
    if(!value.is_null&&(sequence||set||value.container==AttributeContainerKind::kContainer)){
     if(value.children.size()!=listed.size())error("ValueError","Unable to coerce to Series, length must be "+std::to_string(listed.size())+": given "+std::to_string(value.children.size()));
     if(set)error("TypeError","'"+type_name(value)+"' type is unordered");
     for(std::size_t j=0;j<listed.size();++j){
      if(sequence){operands[j]=&value.children[j];continue;}
      operands[j]=&none;
      for(const auto& child:value.children)if(child.key==source.columns[listed[j]]){operands[j]=&child;break;}
     }
    }
    for(std::size_t j=0;j<listed.size();++j)for(std::size_t k=0;k<j;++k)
     if(listed[j]==listed[k])error("ValueError","cannot reindex on an axis with duplicate labels");
    if(alive.empty())alive.assign(source.size(),std::vector<char>(source.columns.size(),1));
    for(const auto row:remaining){
     std::vector<char> next(source.columns.size(),0);
     for(std::size_t j=0;j<listed.size();++j){
      const auto c=listed[j];const auto& v=cell(source,row,c);const auto& item=*operands[j];
      next[c]=alive[row][c]&&!missing(item)&&!missing(v)&&identity(v)==identity(item);
     }
     alive[row]=std::move(next);
    }
    continue;
   }
   const auto stream=direct_column(source,**control.first);
   // Each stream filter compares the rows that remain; a list value needs one
   // item per remaining row.
   if(value.container==AttributeContainerKind::kList&&value.children.size()!=remaining.size())
    error("ValueError","('Lengths must match to compare', ("+std::to_string(remaining.size())+",), ("+std::to_string(value.children.size())+",))");
   if(!source.dtypes.empty())dict_operand(value,remaining.size(),source.dtypes[stream]);
   if(value.container==AttributeContainerKind::kTuple&&!source.dtypes.empty()&&source.dtypes[stream]=="str"){
    if(value.children.size()!=remaining.size())error("ValueError","Lengths of operands do not match: "+std::to_string(remaining.size())+" != "+std::to_string(value.children.size()));
    if(!remaining.empty())error("ValueError","attribute stream tuples are outside the captured native domain");
   }
   for(std::size_t k=0;k<remaining.size();++k){
    const auto& item=value.container==AttributeContainerKind::kList?value.children[k]:value;const auto& v=cell(source,remaining[k],stream);
    selected[remaining[k]]=visible(remaining[k],stream)&&!missing(item)&&!missing(v)&&identity(v)==identity(item);
   }
  }
}
EventTable numeric_filter(const EventTable& source,const EventAttribute& lower,const EventAttribute& upper,const PandasAttributesFilterOptions& options,bool events){
 source.validate();
 if(empty_list(options.attribute_key)){
  // Round CM: df[[]] has no columns, so a list bound must be empty to align
  // (seed 2026111552 case 167).
  // Round CT: a dict bound is list-like too (its keys), and an empty set has
  // no order (fresh CK bank 2026111721 case 433).
  for(const auto* bound:{&lower,&upper}){
   if(bound->is_null||bound->container==AttributeContainerKind::kScalar)continue;
   if(!bound->children.empty())error("ValueError","Unable to coerce to Series, length must be 0: given "+std::to_string(bound->children.size()));
   if(bound->container==AttributeContainerKind::kSet||bound->container==AttributeContainerKind::kFrozenSet)error("TypeError","'"+type_name(*bound)+"' type is unordered");
  }
  if(events)return masked_frame(source);
  // Round DC: the stream filters then read the masked frame, whose cells are
  // all NaN: a key that names no column raises its KeyError, a str key keeps
  // no row, a list key masks nothing more (fresh pandas CK 2026112609 case
  // 175, probe dc/pn1).
  if((options.stream_filter_key1&&!options.stream_filter_key1->is_null)||(options.stream_filter_key2&&!options.stream_filter_key2->is_null)){
   std::vector<bool> selected(source.size(),true);
   std::vector<std::vector<char>> alive(source.size(),std::vector<char>(source.columns.size(),0));
   stream_filters(source,options,selected,alive);
   return project_cases(source,options.case_id_key,selected,truth(options.positive),&alive);
  }
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
 const auto compare_all=[&](const EventAttribute& bound,const std::string& op,std::vector<bool>& out){
  // Round DT (probe dt t1): a numpy column hands a tuple to numpy, which
  // makes it an array first (pandas_numpy_operand.hpp); a None, dict or
  // other leaf that is no number is an object whose comparison Python
  // refuses, NaN rows included.
  if(bound.container==AttributeContainerKind::kTuple&&numpy_number_dtype(dtype)){
   const std::string python=dtype.starts_with("float")?"float":dtype=="bool"?"bool":"int";
   out=detail::numpy_tuple_mask(source.size(),bound,op,numpy_dtype_class(dtype),[&](std::size_t i,const EventAttribute& item){
    if(item.is_null||item.container!=AttributeContainerKind::kScalar||!numeric(item)||date_value(item)||is_native_na_attribute(item))
     error("TypeError","'"+op+"' not supported between instances of '"+python+"' and '"+(item.is_null?std::string("NoneType"):qualified_type_name(item))+"'");
    return comparison(cell(source,i,attribute),item,op,array_item_dtype(dtype));
   },error);
   return;
  }
  // Round DU (probe du v1): a masked column refuses a second dimension and
  // another length before comparing row by row (pandas_numpy_operand.hpp);
  // an item that is pd.NA has no truth value.
  if(bound.container==AttributeContainerKind::kTuple&&masked_number_dtype(dtype)){
   const auto numpy=masked_numpy_dtype(dtype);
   const std::string python=numpy.starts_with("float")?"float":numpy=="bool"?"bool":"int";
   out=detail::masked_tuple_mask(source.size(),bound,op,numpy_dtype_class(numpy),[&](std::size_t i,const EventAttribute& item){
    if(is_native_na_attribute(item))error("TypeError","boolean value of NA is ambiguous");
    if(item.is_null||item.container!=AttributeContainerKind::kScalar||!numeric(item)||date_value(item))
     error("TypeError","'"+op+"' not supported between instances of '"+python+"' and '"+(item.is_null?std::string("NoneType"):qualified_type_name(item))+"'");
    return comparison(cell(source,i,attribute),item,op,array_item_dtype(dtype));
   },error);
   return;
  }
  if(bound.container==AttributeContainerKind::kTuple&&dtype=="str"){out=string_tuple_mask(source,attribute,bound,op);return;}
  if(bound.container==AttributeContainerKind::kTuple&&dtype.starts_with("datetime64")){out=datetime_tuple_mask(source,attribute,bound,op,dtype);return;}
  const auto bounds=row_bounds(bound,source.size(),dtype);
  if(bound.container==AttributeContainerKind::kList&&(numpy_number_dtype(dtype)||masked_number_dtype(dtype))){
   out=list_operand_mask(source,attribute,bound,op,dtype);
   return;
  }
  for(std::size_t i=0;i<source.size();++i)out[i]=comparison(cell(source,i,attribute),bounds[i],op,dtype);
 };
 compare_all(lower,outside?"<":">=",low);
 compare_all(upper,outside?">":"<=",high);
 for(std::size_t i=0;i<source.size();++i){const auto& value=cell(source,i,attribute);const bool within=low[i]&&high[i];selected[i]=events?((outside?(low[i]||high[i]):within)||(keep_nan&&missing(value))):(within||(keep_nan&&missing(value)));}
 if(!events){
  // Round CT: df[list] == value is a frame, so df[mask] masks the rows
  // (DataFrame.where) instead of dropping them: a cell stays only where its
  // column is listed and equals the value, every other cell turns NaN
  // (seed 2026111709 case 120).
  std::vector<std::vector<char>> alive;
  stream_filters(source,options,selected,alive);
  return project_cases(source,options.case_id_key,selected,positive,&alive);
 }
 return events?subset(source,selected):project_cases(source,options.case_id_key,selected,positive,nullptr,true);
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
 // Round CQ: two lists (or two tuples) compare item by item: the first pair
 // that is not equal decides (and may fail), else the longer one is greater.
 const auto sequence=[](const EventAttribute& e){return !e.is_null&&(e.container==AttributeContainerKind::kList||e.container==AttributeContainerKind::kTuple);};
 if(sequence(left)&&sequence(right)&&left.container==right.container){
  const auto n=std::min(left.children.size(),right.children.size());
  for(std::size_t i=0;i<n;++i)if(identity(left.children[i])!=identity(right.children[i]))return greater_equal(left.children[i],right.children[i]);
  return left.children.size()>=right.children.size();
 }
 if(is_native_na_attribute(left)||is_native_na_attribute(right))error("TypeError","boolean value of NA is ambiguous");
 if(numeric(left)&&numeric(right)){if(missing(left)||missing(right))return false;return native_scalar_key_compare(left,right)>=0;}
 // Round CN: two strs compare by code point (UTF-8 bytes keep that order).
 if(text_value(left)&&text_value(right))return left.value>=right.value;
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
 if(relative){
  // Round CP: the case count reads the case key enum first, the per-case
  // grouping the key given last; the event filter reads the attribute key
  // enum first.
  // Round CV: the per-case count reads the keep_once_per_case given last
  // (either key), the denominator KEEP_ONCE_PER_CASE enum first.
  std::optional<EventAttribute> enum_case,string_case,last_case,enum_attribute,string_attribute,enum_once,string_once,last_once;
  for(const auto& p:parameters){
   if(p.value.key=="pm4py:param:case_id_key"){(p.enum_key?enum_case:string_case)=p.value;last_case=p.value;}
   else if(p.value.key=="pm4py:param:attribute_key")(p.enum_key?enum_attribute:string_attribute)=p.value;
   else if(p.value.key=="keep_once_per_case"){(p.enum_key?enum_once:string_once)=p.value;last_once=p.value;}
  }
  if(last_once)result.keep_once_per_case=*last_once;
  if(enum_once||string_once)result.relative_keep_once_enum=enum_once?*enum_once:*string_once;
  if(enum_case)result.case_id_key=*enum_case;
  if(last_case)result.relative_case_id_key_last=*last_case;
  if(enum_attribute||string_attribute)result.relative_attribute_key_enum=enum_attribute?*enum_attribute:*string_attribute;
 }
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
static bool group_case_is_null(const PandasAttributesFilterOptions& o){
 const EventAttribute& key=o.relative_case_id_key_last?*o.relative_case_id_key_last:o.case_id_key;
 return key.is_null&&key.container==AttributeContainerKind::kScalar;
}
EventTable filter_pandas_attributes_relative_occurrence(const EventTable& s,const EventAttribute& stake,const PandasAttributesFilterOptions& o){
 s.validate();
 const bool once=o.keep_once_per_case&&truth(*o.keep_once_per_case);
 const bool case_denominator=o.relative_keep_once_enum?truth(*o.relative_keep_once_enum):!o.keep_once_per_case||once;
 // An empty frame only resolves a scalar attribute column.
 // groupby([case, key]) resolves the case column first, except on a two-row
 // frame, where pandas may take the key list itself as the grouper.
 // Round CK: a dict case key groups the per-case counts by mapping the
 // index labels (int labels never match its str keys), so no case forms.
 // Round CY: counted per event, an attribute column of NaN cells only
 // counts no value, and the events are filtered by the empty list before the
 // case key (the denominator) or the stake is read (observed black-box: no
 // KeyError for a missing case column, no grouper length check; fresh pandas
 // CK 2026112209 case 537). Counted per case, the groupby reads the case key
 // first, and a None key is called there.
 // Round DR: on a two-row frame groupby([case, key]) takes the key list
 // itself as one grouper array (pandas' index replacement) unless a key is
 // a dict, list or tuple, or every key names a column or an index level
 // (None names the default index); a set key is unhashable there, in the
 // membership test or when the array is grouped (probe dr/p1).
 const EventAttribute& replaced_case=o.relative_case_id_key_last?*o.relative_case_id_key_last:o.case_id_key;
 bool replacement=false;
 if(once&&s.size()==2){
  const auto blocks=[](const EventAttribute& k){return !k.is_null&&(k.container==AttributeContainerKind::kContainer||k.container==AttributeContainerKind::kList||k.container==AttributeContainerKind::kTuple);};
  if(!blocks(replaced_case)&&!blocks(o.attribute_key)){
   const auto unhashable_set=[](const EventAttribute& k){return !k.is_null&&k.container==AttributeContainerKind::kSet;};
   if(unhashable_set(replaced_case)||unhashable_set(o.attribute_key))error("TypeError","unhashable type: 'set'");
   const auto listed=[&](const EventAttribute& k){
    if(k.is_null&&k.container==AttributeContainerKind::kScalar){
     if(!s.index)return true;
     return std::find(s.index->names.begin(),s.index->names.end(),std::nullopt)!=s.index->names.end();
    }
    if(k.container!=AttributeContainerKind::kScalar||!text_value(k))return false;
    if(column(s,k))return true;
    return s.index&&std::find(s.index->names.begin(),s.index->names.end(),std::optional<std::string>(k.value))!=s.index->names.end();
   };
   replacement=!(listed(replaced_case)&&listed(o.attribute_key));
  }
 }
 // On an empty frame a None case key groups nothing and fails no call.
 if(once&&!replacement&&s.size()!=0&&group_case_is_null(o))error("TypeError","'NoneType' object is not callable");
 if(!once&&s.size()!=0&&!o.attribute_key.is_null&&o.attribute_key.container==AttributeContainerKind::kScalar)
  if(const auto counted=column(s,o.attribute_key)){
   bool any=false;for(std::size_t i=0;i<s.size()&&!any;++i)any=!missing(cell(s,i,*counted));
   if(!any){
    EventAttribute none{"",""};none.container=AttributeContainerKind::kList;
    auto events=o;if(o.relative_attribute_key_enum)events.attribute_key=*o.relative_attribute_key_enum;
    return values_filter(s,none,events,true);
   }
  }
 const EventAttribute& group_case=o.relative_case_id_key_last?*o.relative_case_id_key_last:o.case_id_key;
 const bool unmatched_mapping=once&&!group_case.is_null&&group_case.container==AttributeContainerKind::kContainer&&
  (!s.index||std::all_of(s.index->labels.begin(),s.index->labels.end(),[](const auto& label){const auto* scalar=std::get_if<EventAttribute>(&label.value);return scalar&&!text_value(*scalar);}));
 // Round CV: a list case key is no label, so the groupby takes it as a
 // grouper array of its items (any items, an empty list too), which must
 // match the rows; a tuple is a label that must hash. Neither lets a
 // two-row frame take the key list itself as the grouper.
 // Round DB: a set case key is called on each index label (probe
 // set-pdtblck-2 cases 151, 232).
 if(once&&!group_case.is_null&&(group_case.container==AttributeContainerKind::kSet||group_case.container==AttributeContainerKind::kFrozenSet)&&s.size()!=0)
  error("TypeError","'"+type_name(group_case)+"' object is not callable");
 const bool list_grouper=once&&!unmatched_mapping&&group_case.container==AttributeContainerKind::kList;
 if(list_grouper&&group_case.children.size()!=s.size())error("ValueError","Grouper and axis must be same length");
 if(once&&!unmatched_mapping&&group_case.container==AttributeContainerKind::kTuple)detail::pandas_check_hashable_key(group_case,error);
 // Round DR: so a dict, list or tuple attribute key lets a two-row frame
 // resolve the case column too (fresh pandas CK 2026121109 case 255).
 if(once&&!unmatched_mapping&&!list_grouper&&!replacement&&!(s.size()==0&&group_case_is_null(o)))
  direct_column(s,group_case);
 // Round DR: grouping by the case column twice (the attribute key names
 // it too) gives reset_index two columns of that name, on any frame
 // (probe dr/p1).
 if(once&&!unmatched_mapping&&!list_grouper&&text_value(group_case)&&text_value(o.attribute_key)&&group_case.value==o.attribute_key.value&&column(s,group_case))
  error("ValueError","cannot insert "+group_case.value+", already exists");
 // Round CK: df[key] refuses a dict before it looks at the rows. Round CP:
 // on an empty frame the per-event path reads df[key] (a list selects its
 // columns, a tuple is one label), the per-case path groups by the key (a
 // list is one grouper array, a tuple one label that must hash).
 if(s.size()==0){
  const auto& key=o.attribute_key;
  if(key.container==AttributeContainerKind::kScalar){direct_column(s,key);return s;}
  if(key.container==AttributeContainerKind::kList){
   if(once){if(!key.children.empty())error("ValueError","Grouper and axis must be same length");}
   else check_list_columns(s,key);
   return s;
  }
  if(key.container==AttributeContainerKind::kTuple){
   if(once)detail::pandas_check_hashable_key(key,error);
   else {
    for(const auto& child:key.children){
     if(child.container==AttributeContainerKind::kContainer)error("TypeError","Passing a dict as an indexer is not supported. Use a list instead.");
     if(child.container==AttributeContainerKind::kSet)error("TypeError","Passing a set as an indexer is not supported. Use a list instead.");
    }
    if(detail::pandas_unhashable_type(key))error("InvalidIndexError",repr(key));
   }
   error("KeyError",repr(key));
  }
  detail::pandas_check_getitem_key(key,error);
  return s;
 }
 // After the index replacement the grouped frame is indexed by the key:
 // df[None] is a missing column.
 if(once&&o.attribute_key.is_null&&replacement)error("KeyError","None");
 if(once&&o.attribute_key.is_null)error("TypeError","'NoneType' object is not callable");
 // Round DB: per case, groupby calls a set key on each index label (probe
 // set-pdtblck-2 case 116).
 if(once&&(o.attribute_key.container==AttributeContainerKind::kSet||o.attribute_key.container==AttributeContainerKind::kFrozenSet))
  error("TypeError","'"+type_name(o.attribute_key)+"' object is not callable");
 // A list key is one grouper array.
 if(once&&o.attribute_key.container==AttributeContainerKind::kList){
  if(o.attribute_key.children.size()!=s.size())error("ValueError","Grouper and axis must be same length");
  // Round DR: the grouped frame (one row per case and array item, each
  // column's first non-null cell) keeps every column, so df[list] reads
  // the listed columns' tuples; the denominator and the stake are read once
  // a tuple counts, and the events are filtered on tuples no cell holds
  // (probe dr/p1).
  if(!unmatched_mapping&&!list_grouper&&!replacement){
   check_list_columns(s,o.attribute_key);
   std::vector<std::size_t> listed;for(const auto& item:o.attribute_key.children)listed.push_back(*column(s,item));
   const auto cases=direct_column(s,group_case);
   std::unordered_map<std::string,std::vector<std::size_t>> groups;
   for(std::size_t i=0;i<s.size();++i){
    const auto& item=o.attribute_key.children[i];
    if(missing(cell(s,i,cases))||missing(item))continue;
    groups[group_key(cell(s,i,cases))+'\x1f'+group_key(item)].push_back(i);
   }
   bool counted=false;
   for(const auto& [key,rows]:groups){
    bool complete=true;
    for(const auto c:listed)complete&=std::any_of(rows.begin(),rows.end(),[&](std::size_t i){return !missing(cell(s,i,c));});
    counted|=complete;
   }
   if(counted){
    if(case_denominator){
     if(o.case_id_key.container==AttributeContainerKind::kList){
      check_list_columns(s,o.case_id_key);
      error("ValueError","The truth value of a Series is ambiguous. Use a.empty, a.bool(), a.item(), a.any() or a.all().");
     }
     direct_column(s,o.case_id_key);
    }
    if(stake.is_null||stake.container==AttributeContainerKind::kContainer||stake.container==AttributeContainerKind::kSet||stake.container==AttributeContainerKind::kFrozenSet||(stake.container==AttributeContainerKind::kScalar&&date_value(stake)))
     error("TypeError","unsupported operand type(s) for *: '"+qualified_type_name(stake)+"' and 'int'");
    if(is_native_na_attribute(stake))error("TypeError","boolean value of NA is ambiguous");
    if(!numeric(stake))error("TypeError","'>=' not supported between instances of 'int' and '"+qualified_type_name(stake)+"'");
   }
   EventAttribute values{"",""};values.container=AttributeContainerKind::kList;
   if(!o.relative_attribute_key_enum)return values_filter(s,values,o,true);
   auto events=o;events.attribute_key=*o.relative_attribute_key_enum;
   return values_filter(s,values,events,true);
  }
 }
 if(empty_list(o.attribute_key))error("ValueError","No group keys passed!");
 // Round DR: counted per event, a list key counts df[list].value_counts(),
 // the rows' tuples (a row with a missing listed cell drops); the
 // denominator and the stake are read only once a tuple counts; the
 // events are then filtered on the tuples, which no cell holds (probe
 // dr/p1).
 if(!once&&o.attribute_key.container==AttributeContainerKind::kList){
  check_list_columns(s,o.attribute_key);
  std::vector<std::size_t> listed;for(const auto& item:o.attribute_key.children)listed.push_back(*column(s,item));
  struct Tuple {EventAttribute value;std::size_t count=0;};
  std::vector<Tuple> tuples;std::unordered_map<std::string,std::size_t> seen;
  for(std::size_t i=0;i<s.size();++i){
   EventAttribute row{"",""};row.container=AttributeContainerKind::kTuple;bool complete=true;
   for(const auto c:listed){const auto& v=cell(s,i,c);if(missing(v)){complete=false;break;}row.children.push_back(v);}
   if(!complete)continue;
   std::string key;for(const auto& v:row.children)key+=group_key(v)+'\x1f';
   const auto [it,added]=seen.emplace(key,tuples.size());if(added)tuples.push_back({row});
   ++tuples[it->second].count;
  }
  std::size_t denominator=s.size();
  if(!tuples.empty()&&case_denominator&&!unmatched_mapping){
   if(o.case_id_key.container==AttributeContainerKind::kList){
    check_list_columns(s,o.case_id_key);
    error("ValueError","The truth value of a Series is ambiguous. Use a.empty, a.bool(), a.item(), a.any() or a.all().");
   }
   const auto cases=direct_column(s,o.case_id_key);std::unordered_set<std::string> ids;
   for(std::size_t i=0;i<s.size();++i)if(!missing(cell(s,i,cases)))ids.insert(group_key(cell(s,i,cases)));
   denominator=ids.size();
  }
  if(!tuples.empty()&&(stake.is_null||stake.container==AttributeContainerKind::kContainer||stake.container==AttributeContainerKind::kSet||stake.container==AttributeContainerKind::kFrozenSet||(stake.container==AttributeContainerKind::kScalar&&date_value(stake))))
   error("TypeError","unsupported operand type(s) for *: '"+qualified_type_name(stake)+"' and 'int'");
  if(!tuples.empty()&&is_native_na_attribute(stake))error("TypeError","boolean value of NA is ambiguous");
  if(!numeric(stake)&&!tuples.empty())error("TypeError","'>=' not supported between instances of 'int' and '"+qualified_type_name(stake)+"'");
  const double threshold=!numeric(stake)?0.0:native_attribute_number(stake)*static_cast<double>(denominator);
  EventAttribute values{"",""};values.container=AttributeContainerKind::kList;
  for(const auto& t:tuples)if(static_cast<double>(t.count)>=threshold)values.children.push_back(t.value);
  if(!o.relative_attribute_key_enum)return values_filter(s,values,o,true);
  auto events=o;events.attribute_key=*o.relative_attribute_key_enum;
  return values_filter(s,values,events,true);
 }
 const auto attribute=direct_column(s,o.attribute_key);
 std::optional<std::size_t> cases,grouped;std::unordered_set<std::string> all_cases;
 if(case_denominator&&!unmatched_mapping){
  // Round CV: df[list].nunique() is a Series, which the stake comparison
  // cannot take as a truth value.
  if(o.case_id_key.container==AttributeContainerKind::kList){
   check_list_columns(s,o.case_id_key);
   error("ValueError","The truth value of a Series is ambiguous. Use a.empty, a.bool(), a.item(), a.any() or a.all().");
  }
  cases=direct_column(s,o.case_id_key);
  for(std::size_t i=0;i<s.size();++i)if(!missing(cell(s,i,*cases)))all_cases.insert(group_key(cell(s,i,*cases)));
  if(once&&!list_grouper)grouped=o.relative_case_id_key_last?direct_column(s,group_case):*cases;
 }
 if(once&&!list_grouper&&!unmatched_mapping&&!grouped)grouped=direct_column(s,group_case);
 // A list grouper's items are the rows' cases.
 const auto grouped_cell=[&](std::size_t i)->const EventAttribute&{return list_grouper?group_case.children[i]:cell(s,i,*grouped);};
 struct Frequency {EventAttribute value;std::size_t count=0;std::unordered_set<std::string> cases;};
 std::vector<Frequency> frequencies;std::unordered_map<std::string,std::size_t> positions;
 for(std::size_t i=0;i<s.size()&&!unmatched_mapping;++i){const auto& value=cell(s,i,attribute);if(missing(value))continue;
  // Per-case counts group by (case, value), which drops null cases.
  if(once&&missing(grouped_cell(i)))continue;
  const auto key=group_key(value);const auto [position,added]=positions.emplace(key,frequencies.size());if(added)frequencies.push_back({value});
  auto& f=frequencies[position->second];if(!once)++f.count;else f.cases.insert(group_key(grouped_cell(i)));
 }
 // stake * count: only numbers and sequences multiply by an int; round CM:
 // the product is formed per counted value, so no value means no error
 // (seed 2026111552 case 163: a one-row frame with a NaN activity).
 if(!frequencies.empty()&&(stake.is_null||stake.container==AttributeContainerKind::kContainer||stake.container==AttributeContainerKind::kSet||stake.container==AttributeContainerKind::kFrozenSet||(stake.container==AttributeContainerKind::kScalar&&date_value(stake))))
  error("TypeError","unsupported operand type(s) for *: '"+qualified_type_name(stake)+"' and 'int'");
 if(!frequencies.empty()&&is_native_na_attribute(stake))error("TypeError","boolean value of NA is ambiguous");
 if(!numeric(stake)&&!frequencies.empty())error("TypeError","'>=' not supported between instances of 'int' and '"+qualified_type_name(stake)+"'");
 const double threshold=!numeric(stake)?0.0:native_attribute_number(stake)*static_cast<double>(case_denominator?all_cases.size():s.size());
 std::vector<EventAttribute> admitted;for(const auto& f:frequencies)if(static_cast<double>(once?f.cases.size():f.count)>=threshold)admitted.push_back(f.value);
 EventAttribute values{"",""};values.container=AttributeContainerKind::kList;values.children=std::move(admitted);
 if(!o.relative_attribute_key_enum)return values_filter(s,values,o,true);
 auto events=o;events.attribute_key=*o.relative_attribute_key_enum;
 return values_filter(s,values,events,true);
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
