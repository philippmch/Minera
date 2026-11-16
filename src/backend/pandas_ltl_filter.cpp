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
#include "python_scalar.hpp"
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
 if(v.container==AttributeContainerKind::kContainer||v.container==AttributeContainerKind::kList)
  return detail::pandas_container_repr(v,[](const EventAttribute& x){return repr(x);},[](const std::string& k){return string_key_repr(k);});
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
 // Round CK: a dict is subscripted by key; its keys are strs, never the int.
 if(value.container==AttributeContainerKind::kContainer&&!value.is_null)error("KeyError",std::to_string(position));
 // Round CR: a set is not subscriptable; a tuple and a str name themselves
 // in the IndexError.
 if((value.container==AttributeContainerKind::kSet||value.container==AttributeContainerKind::kFrozenSet)&&!value.is_null)
  error("TypeError","'"+type_name(value)+"' object is not subscriptable");
 const auto values=iterable(value);
 if(position>=values.size())
  error("IndexError",std::string(value.container==AttributeContainerKind::kTuple?"tuple":value.container==AttributeContainerKind::kScalar?"string":"list")+" index out of range");
 return values[position];
}
bool equal_value(const EventAttribute& a,const EventAttribute& b){return identity(a)==identity(b);}
bool matches(const EventAttribute& value,const EventAttribute& target){return !missing(value)&&!missing(target)&&equal_value(value,target);}
// Round CK: a list target with one item per row compares item by item.
bool matches_at(const EventAttribute& value,const EventAttribute& target,std::size_t row){
 if(!target.is_null&&target.container==AttributeContainerKind::kList&&row<target.children.size())return matches(value,target.children[row]);
 return matches(value,target);
}
// Round DG (fresh pandas CK 2026113009 case 469): a str column compares a
// tuple of one item per row element-wise as numpy holds the tuple. With a
// str among scalars of str, bool, float and ints in the UInt64 range it is a
// str array (each item as str() gives it, so 1 equals "1"); with a None,
// NaN-free dict or a larger int an object array (Python ==); with numbers
// only a number array that no str equals. A tuple holding a sequence, a
// date or another value stays refused.
enum class RowTuple{kNone,kText,kObject,kNumber};
bool tuple_int_fits(const EventAttribute& v){
 if(std::holds_alternative<std::int64_t>(v.typed_value)||std::holds_alternative<std::uint64_t>(v.typed_value))return true;
 const auto& t=v.value;
 const bool negative=!t.empty()&&t[0]=='-';
 const auto digits=t.substr(negative?1:0);
 if(digits.empty()||digits.find_first_not_of("0123456789")!=std::string::npos)return false;
 const std::string limit=negative?"9223372036854775808":"18446744073709551615";
 return digits.size()<limit.size()||(digits.size()==limit.size()&&digits<=limit);
}
RowTuple row_tuple(const EventAttribute& target){
 bool text=false,object=false;
 for(const auto& item:target.children){
  if(item.container==AttributeContainerKind::kContainer&&!item.is_null){object=true;continue;}
  if(item.container!=AttributeContainerKind::kScalar)return RowTuple::kNone;
  if(item.is_null&&!date_value(item)&&!is_native_na_attribute(item)){object=true;continue;}
  if(item.is_null||is_native_na_attribute(item)||date_value(item))return RowTuple::kNone;
  if(text_value(item)){text=true;continue;}
  if(item.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(item.typed_value))continue;
  if(item.kind==AttributeKind::kFloat||std::holds_alternative<double>(item.typed_value))continue;
  if(item.kind==AttributeKind::kInt){if(!tuple_int_fits(item))object=true;continue;}
  return RowTuple::kNone;
 }
 return object?RowTuple::kObject:text?RowTuple::kText:RowTuple::kNumber;
}
// The tuple as a list of one target per row (see row_tuple).
EventAttribute row_targets(const EventAttribute& target){
 const auto kind=row_tuple(target);
 EventAttribute out=target;
 out.container=AttributeContainerKind::kList;
 for(auto& item:out.children){
  if(kind==RowTuple::kObject)continue;
  EventAttribute cell;cell.key=item.key;
  if(kind==RowTuple::kNumber){cell.is_null=true;item=cell;continue;}
  if(text_value(item))continue;
  if(item.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(item.typed_value)){
   const bool b=std::holds_alternative<bool>(item.typed_value)?std::get<bool>(item.typed_value):(item.value=="true"||item.value=="True"||item.value=="1");
   cell.value=b?"True":"False";
  }else if(item.kind==AttributeKind::kFloat||std::holds_alternative<double>(item.typed_value)){
   cell.value=python::float_repr(native_attribute_number(item));
  }else if(const auto* i=std::get_if<std::int64_t>(&item.typed_value))cell.value=std::to_string(*i);
  else if(const auto* u=std::get_if<std::uint64_t>(&item.typed_value))cell.value=std::to_string(*u);
  else cell.value=item.value;
  item=cell;
 }
 return out;
}
void check_target(const EventAttribute& target,std::size_t rows,const std::string& dtype){
 // Round CK: a str or datetime64 column compares a dict as an array of its
 // keys, which numpy then holds as one 0-d object. Round CZ: a set or a
 // frozenset compares the same way over its items.
 const auto kind=target.container;
 if((kind==AttributeContainerKind::kContainer||kind==AttributeContainerKind::kSet||kind==AttributeContainerKind::kFrozenSet)&&!target.is_null){
  const auto n=target.children.size();
  if(dtype=="str"){
   if(n!=rows)error("ValueError","Lengths of operands do not match: "+std::to_string(rows)+" != "+std::to_string(n));
   error("IndexError","too many indices for array: array is 0-dimensional, but 1 were indexed");
  }
  if(dtype.starts_with("datetime64")){
   if(n!=rows)error("ValueError","Lengths must match");
   error("ValueError","Buffer has wrong number of dimensions (expected 1, got 0)");
  }
 }
 if(target.container==AttributeContainerKind::kList&&target.children.size()!=rows)
  error("ValueError","('Lengths must match to compare', ("+std::to_string(rows)+",), ("+std::to_string(target.children.size())+",))");
 // A str column compares a tuple as an array.
 if(target.container==AttributeContainerKind::kTuple&&dtype=="str"){
  if(target.children.size()!=rows)error("ValueError","Lengths of operands do not match: "+std::to_string(rows)+" != "+std::to_string(target.children.size()));
  if(rows&&row_tuple(target)==RowTuple::kNone)error("ValueError","ltl filter vector targets are outside the captured native domain");
 }
}
std::vector<std::size_t> selected_columns(const EventTable& source,const std::vector<EventAttribute>& requested){
 std::vector<std::size_t> result;std::vector<std::string> absent;std::vector<std::string> names;std::size_t missing_count=0;
 // Round CK: the keys hash before the columns are read.
 for(const auto& key:requested)detail::pandas_check_hashable_key(key,error);
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
bool temporal_bound(double value,const EventAttribute& bound,bool lower,bool eager=false){
 if(missing(bound))return false;
 // Round CK: a dict bound compares row by row, so only a step with rows
 // fails (Python's own comparison error).
 if(bound.container==AttributeContainerKind::kContainer&&!bound.is_null){
  if(eager)return false;
  error("TypeError",std::string(lower?"'>='":"'<='")+" not supported between instances of 'float' and 'dict'");
 }
 if(!numeric(bound))error("TypeError","Invalid comparison between dtype=float64 and "+type_name(bound));
 const auto number=native_attribute_number(bound);
 return lower?value>=number:value<=number;
}
// Round CN: a list bound compares item by item with the step's time
// differences, one per chain of matching events (every later event of each
// step's activity in the case, by row position, in merge order) that the
// earlier steps kept. A list of another length fails to compare; an item
// that is not a number fails at the first such item (probe pd2-pd5).
bool listed_bound(const EventAttribute& value){return !value.is_null&&value.container==AttributeContainerKind::kList;}
bool listed_bounds(const EventAttribute& boundaries,std::size_t steps){
 const auto sequence=[](const EventAttribute& v){return !v.is_null&&(v.container==AttributeContainerKind::kList||v.container==AttributeContainerKind::kTuple);};
 if(!sequence(boundaries))return false;
 for(std::size_t i=0;i+1<steps&&i<boundaries.children.size();++i){
  const auto& bound=boundaries.children[i];
  if(!sequence(bound))continue;
  for(std::size_t j=0;j<2&&j<bound.children.size();++j)if(listed_bound(bound.children[j]))return true;
 }
 return false;
}
std::vector<bool> compare_step(const std::vector<double>& differences,const EventAttribute& bound,bool lower){
 std::vector<bool> result(differences.size(),false);
 if(listed_bound(bound)){
  const auto n=bound.children.size();
  if(n!=differences.size())error("ValueError","('Lengths must match to compare', ("+std::to_string(differences.size())+",), ("+std::to_string(n)+",))");
  const std::string op=lower?"'>='":"'<='",reflected=lower?"'<='":"'>='";
  // pandas infers the list: strs (and Nones) make a str array, compared the
  // other way round; numbers (no bools) and Nones a float array (None as
  // NaN); anything else an object array, compared item by item (probe pd6).
  const auto& items=bound.children;
  const auto plain_null=[](const EventAttribute& x){return x.is_null&&!date_value(x)&&x.container==AttributeContainerKind::kScalar;};
  const auto number=[](const EventAttribute& x){return !x.is_null&&x.container==AttributeContainerKind::kScalar&&numeric(x)&&type_name(x)!="bool";};
  const bool any_text=std::any_of(items.begin(),items.end(),[](const EventAttribute& x){return text_value(x);});
  if(any_text&&std::all_of(items.begin(),items.end(),[&](const EventAttribute& x){return text_value(x)||plain_null(x);}))
   error("TypeError",reflected+" not supported between instances of 'str' and 'float'");
  const bool floats=std::any_of(items.begin(),items.end(),number)&&std::all_of(items.begin(),items.end(),[&](const EventAttribute& x){return number(x)||plain_null(x);});
  for(std::size_t j=0;j<n;++j){
   const auto& item=items[j];
   if(floats&&item.is_null)continue;
   if(!item.is_null&&item.container==AttributeContainerKind::kScalar&&numeric(item)){
    const auto value=native_attribute_number(item);
    result[j]=lower?differences[j]>=value:differences[j]<=value;
    continue;
   }
   error("TypeError",op+" not supported between instances of 'float' and '"+type_name(item)+"'");
  }
  return result;
 }
 if(missing(bound))return result;
 if(bound.container==AttributeContainerKind::kContainer&&!bound.is_null&&differences.empty())return result;
 for(std::size_t j=0;j<differences.size();++j)result[j]=temporal_bound(differences[j],bound,lower);
 if(differences.empty())temporal_bound(0.0,bound,lower,true);
 return result;
}
bool dict_bounds(const EventAttribute& boundaries,std::size_t steps){
 if(boundaries.is_null||(boundaries.container!=AttributeContainerKind::kList&&boundaries.container!=AttributeContainerKind::kTuple))return false;
 for(std::size_t i=0;i+1<steps&&i<boundaries.children.size();++i){
  const auto& bound=boundaries.children[i];
  if(bound.is_null||(bound.container!=AttributeContainerKind::kList&&bound.container!=AttributeContainerKind::kTuple))continue;
  for(std::size_t j=0;j<2&&j<bound.children.size();++j)if(!bound.children[j].is_null&&bound.children[j].container==AttributeContainerKind::kContainer)return true;
 }
 return false;
}
std::unordered_set<std::string> listed_steps(const EventTable& source,const std::vector<EventAttribute>& anchors,const std::vector<std::size_t>& columns,const std::vector<std::size_t>& valid,const EventAttribute& boundaries){
 // Every chain is built first (merge order: by the first event's row, then
 // the next one's), then each step keeps the chains within its bounds.
 std::vector<std::vector<std::size_t>> chains;
 for(const auto row:valid)if(matches_at(cell(source,row,columns[1]),anchors[0],row))chains.push_back({row});
 for(std::size_t i=0;i+1<anchors.size();++i){
  std::vector<std::vector<std::size_t>> next;
  for(const auto& chain:chains){
   const auto last=chain.back();
   const auto key=group_key(cell(source,last,columns[0]));
   for(const auto row:valid){
    if(row<=last||group_key(cell(source,row,columns[0]))!=key||!matches_at(cell(source,row,columns[1]),anchors[i+1],row))continue;
    auto longer=chain;longer.push_back(row);next.push_back(std::move(longer));
   }
  }
  chains=std::move(next);
 }
 // The lower bound filters the chains before the upper one is compared.
 for(std::size_t i=0;i+1<anchors.size();++i){
  const auto bound=element(boundaries,i);
  for(std::size_t side=0;side<2;++side){
   std::vector<double> differences;
   for(const auto& chain:chains)differences.push_back(duration(cell(source,chain[i],columns.back()),cell(source,chain[i+1],columns.back())));
   const auto within=compare_step(differences,element(bound,side),side==0);
   std::vector<std::vector<std::size_t>> kept;
   for(std::size_t j=0;j<chains.size();++j)if(within[j])kept.push_back(std::move(chains[j]));
   chains=std::move(kept);
  }
 }
 std::unordered_set<std::string> result;
 for(const auto& chain:chains)result.insert(group_key(cell(source,chain.front(),columns[0])));
 return result;
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
 // The resource naming the case column fails first, when the case column
 // is set from the selection (round BZ: also with all three keys equal).
 if(resources&&columns[0]==columns[2])error("ValueError","Cannot set a DataFrame with multiple columns to the single column @@conceptname");
 // Round CK: when the attribute and resource columns are one label, the
 // attribute reads as a two-column frame, which a dict or list as the first
 // target, of another size, cannot be coerced against; the frame fails to
 // reindex before the second target is compared.
 if(columns[0]!=columns[1]&&resources&&columns[1]==columns[2]&&!anchors.empty()){
  const auto& target=anchors.front();
  // Round CR: a tuple too (fresh CK bank 2026111653 case 167).
  // Round CZ: a set or a frozenset too, which of size 2 has no order.
  const bool set=target.container==AttributeContainerKind::kSet||target.container==AttributeContainerKind::kFrozenSet;
  if((target.container==AttributeContainerKind::kContainer||target.container==AttributeContainerKind::kList||target.container==AttributeContainerKind::kTuple||set)&&!target.is_null&&target.children.size()!=2)
   error("ValueError","Unable to coerce to Series, length must be 2: given "+std::to_string(target.children.size()));
  if(set&&!target.is_null)error("TypeError","'"+std::string(target.container==AttributeContainerKind::kSet?"set":"frozenset")+"' type is unordered");
 }
 if(columns[0]==columns[1]||(resources&&columns[1]==columns[2]))error("ValueError","cannot reindex on an axis with duplicate labels");
 // With timestamp differences the timestamp key joins the selection: on the
 // activity column it duplicates a label, on the case column the sort by
 // case and time meets two columns of one label (round BZ).
 if(timed){
  const auto time=columns.back();
  if(time==columns[1])error("ValueError","cannot reindex on an axis with duplicate labels");
  if(time==columns[0])error("ValueError","The column label '"+source.columns[time]+"' is not unique.");
 }
 std::vector<std::size_t> valid;
 for(std::size_t i=0;i<source.size();++i){bool good=true;for(const auto c:columns)if(missing(cell(source,i,c))){good=false;break;}if(good)valid.push_back(i);}
 for(auto& target:anchors){
  const auto dtype=source.dtypes.empty()?"object":source.dtypes[columns[1]];
  check_target(target,source.size(),dtype);
  if(target.container==AttributeContainerKind::kTuple&&!target.is_null&&dtype=="str"&&source.size())target=row_targets(target);
 }
 std::optional<std::unordered_set<std::string>> listed;
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
   // Round CS: a dict bound compares row by row after every chain is
   // built, so a step only fails when whole chains reach it (fresh pandas
   // CK bank 2026111679 case 738).
   if(listed_bounds(boundaries,anchors.size())||dict_bounds(boundaries,anchors.size()))listed=listed_steps(source,anchors,columns,valid,boundaries);
   else for(std::size_t i=0;i+1<anchors.size();++i){
    const auto bound=element(boundaries,i);
    temporal_bound(0.0,element(bound,0),true,true);temporal_bound(0.0,element(bound,1),false,true);
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
     if(!matches_at(cell(source,row,activity_column),anchors[depth],row))continue;
     if(depth&&timed&&boundary_count){
      const auto bound=element(boundaries,depth-1);
      const auto elapsed=duration(cell(source,previous,columns.back()),cell(source,row,columns.back()));
      const bool above=temporal_bound(elapsed,element(bound,0),true),below=temporal_bound(elapsed,element(bound,1),false);
      if(!above||!below)continue;
     }
     if(self(self,depth+1,i+1,row))return true;
    }
    return false;
   };
   if(listed?listed->contains(key):search(search,0,0,0))selected.insert(key);
  }else if(resources){
   std::vector<const EventAttribute*> a,b;
   for(const auto row:rows){if(matches_at(cell(source,row,activity_column),anchors[0],row))a.push_back(&cell(source,row,columns[2]));if(rule==Rule::kFourEyes&&matches_at(cell(source,row,activity_column),anchors[1],row))b.push_back(&cell(source,row,columns[2]));}
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
    if(matches_at(cell(source,row,activity_column),anchors[1],row))b=row;
    if(matches_at(cell(source,row,activity_column),anchors[2],row))c=row;
   }
   if(!b||!c||*b==0||*c!=*b+1)continue;
   const auto a=*b-1;
   if(!missing(cell(source,a,columns[0]))&&!missing(cell(source,a,activity_column))&&
      key==group_key(cell(source,a,columns[0]))&&matches_at(cell(source,a,activity_column),anchors[0],a))selected.insert(key);
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
