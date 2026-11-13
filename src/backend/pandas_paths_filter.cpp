#include "minera/pandas_paths_filter.hpp"
#include "pandas_parameter_carrier.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string_view>
#include <unordered_set>
#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"
namespace minera { namespace {
[[noreturn]] void error(std::string kind,std::string message){throw PandasPathsFilterError(std::move(kind),std::move(message));}
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
    else error("ValueError", "pandas between selector has invalid UTF-8");
    for (unsigned i = 0; i < continuation; ++i) {
      if (position == value.size()) error("ValueError", "pandas between selector has short UTF-8");
      const auto next = static_cast<unsigned char>(value[position++]);
      if ((next & 0xc0) != 0x80) error("ValueError", "pandas between selector has invalid UTF-8 continuation");
      code = (code << 6) | (next & 63);
    }
    if ((continuation == 1 && code < 0x80) ||
        (continuation == 2 && code < 0x800) ||
        (continuation == 3 && code < 0x10000) || code > 0x10ffff)
      error("ValueError", "pandas between selector has invalid code point");
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
 // Native date precision does not encode a Python scalar class. The captured
 // path/query date operands are pandas Timestamp scalars at both us and ns
 // resolution; retain their class name independently of their precision.
 if(date_value(v))return std::holds_alternative<NativeDate>(v.typed_value)?"Timestamp":"datetime.datetime";
 if(v.kind==AttributeKind::kBytes)return "bytes";
 if(v.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(v.typed_value))return "bool";
 if(v.kind==AttributeKind::kInt||std::holds_alternative<std::int64_t>(v.typed_value)||std::holds_alternative<std::uint64_t>(v.typed_value))return "int";
 return numeric(v)?"float":"str";
}
std::string repr(const EventAttribute& v){
 if(text_value(v))return string_key_repr(v.value);
 if(v.container==AttributeContainerKind::kContainer){std::string result="{";for(std::size_t i=0;i<v.children.size();++i){if(i)result+=", ";result+=string_key_repr(v.children[i].key)+": "+repr(v.children[i]);}return result+"}";}
 if(v.container==AttributeContainerKind::kList){std::string result="[";for(std::size_t i=0;i<v.children.size();++i){if(i)result+=", ";result+=repr(v.children[i]);}return result+"]";}
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
std::string group_key(const EventAttribute& v){
 if(!root_cause_key_is_hashable(v))error("TypeError","unhashable type: '"+type_name(v)+"'");
 if(date_value(v)){
  const auto t=instant(v);if(!t)return "missing";
  return std::string(t->timezone_aware?"DA":"DN")+std::to_string(t->seconds)+":"+std::to_string(t->nanoseconds);
 }
 return root_cause_scalar_key_fingerprint(v);
}

std::optional<std::size_t> find_column(const EventTable& s,const EventAttribute& key){
 if(!text_value(key))return std::nullopt;
 const auto p=std::find(s.columns.begin(),s.columns.end(),key.value);
 return p==s.columns.end()?std::nullopt:std::optional<std::size_t>(p-s.columns.begin());
}
std::size_t sort_column(const EventTable& s,const EventAttribute& key){
 if(const auto p=find_column(s,key))return *p;
 error("KeyError",repr(key));
}
std::size_t activity_column(const EventTable& s,const EventAttribute& key){
 if(!root_cause_key_is_hashable(key))error("TypeError","unhashable type: '"+type_name(key)+"'");
 if(const auto p=find_column(s,key))return *p;
 error("KeyError",string_key_repr("["+(key.is_null?std::string("nan"):repr(key))+"] not in index"));
}
const EventAttribute& cell(const EventTable& s,std::size_t row,std::size_t col){
 static const EventAttribute null{"","",AttributeKind::kString,true};
 return s.rows[row][col]?*s.rows[row][col]:null;
}
int compare(const EventAttribute& a,const EventAttribute& b){
 const bool am=missing(a),bm=missing(b);if(am||bm)return am==bm?0:am?1:-1;
 if(a.container==AttributeContainerKind::kTuple&&b.container==AttributeContainerKind::kTuple){
  for(std::size_t i=0;i<std::min(a.children.size(),b.children.size());++i){const auto c=compare(a.children[i],b.children[i]);if(c)return c;}
  return a.children.size()<b.children.size()?-1:a.children.size()>b.children.size()?1:0;
 }
 const auto ak=native_scalar_order_kind(a),bk=native_scalar_order_kind(b);
 // pandas sorts datetime64[ns] cells by the exact instant: two cells
 // nanoseconds apart sort apart (round BW). The scalar-key order compares
 // microseconds, so dates of one awareness compare here.
 if(ak==NativeScalarOrderKind::kDate&&bk==ak){
  const auto x=instant(a),y=instant(b);
  if(x&&y&&x->timezone_aware==y->timezone_aware){
   if(x->seconds!=y->seconds)return x->seconds<y->seconds?-1:1;
   return x->nanoseconds<y->nanoseconds?-1:x->nanoseconds>y->nanoseconds?1:0;
  }
 }
 if(ak==bk&&ak!=NativeScalarOrderKind::kUnsupported)return native_scalar_key_compare(a,b);
 const auto rank=[](NativeScalarOrderKind k){return k==NativeScalarOrderKind::kNumeric?0:k==NativeScalarOrderKind::kDate?1:k==NativeScalarOrderKind::kString?2:k==NativeScalarOrderKind::kBytes?3:4;};
 if(rank(ak)!=rank(bk))return rank(ak)<rank(bk)?-1:1;
 error("TypeError","'<' not supported between instances of '"+type_name(a)+"' and '"+type_name(b)+"'");
}
bool same_case(const EventAttribute& a,const EventAttribute& b){return !missing(a)&&!missing(b)&&group_key(a)==group_key(b);}
std::vector<std::size_t> sorted_rows(const EventTable& s,std::size_t cases,std::size_t timestamp){
 for(const auto& row:s.rows)if(row[cases]&&!missing(*row[cases]))(void)group_key(*row[cases]);
 std::vector<std::size_t> order(s.size());std::iota(order.begin(),order.end(),0);
 const EventTableCategoricalDtype* category=!s.categorical_dtypes.empty()&&s.categorical_dtypes[cases]?&*s.categorical_dtypes[cases]:nullptr;
 const auto category_rank=[&](const EventAttribute& a){
  if(missing(a))return category->categories.size();
  for(std::size_t i=0;i<category->categories.size();++i)if(root_cause_scalar_keys_equal(a,category->categories[i]))return i;
  return category->categories.size();
 };
 std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){
  int c=0;
  if(category){const auto x=category_rank(cell(s,a,cases)),y=category_rank(cell(s,b,cases));c=x<y?-1:x>y?1:0;}
  else c=compare(cell(s,a,cases),cell(s,b,cases));
  return c?c<0:compare(cell(s,a,timestamp),cell(s,b,timestamp))<0;
 });
 return order;
}
bool arithmetic(const std::vector<EventTableIndexLabel>& labels){
 std::vector<long long> numbers;
 for(const auto& label:labels){const auto* a=std::get_if<EventAttribute>(&label.value);if(!a||a->is_null)return false;
  if(const auto* v=std::get_if<std::int64_t>(&a->typed_value))numbers.push_back(*v);
  else if(const auto* v=std::get_if<std::uint64_t>(&a->typed_value)){if(*v>std::uint64_t(std::numeric_limits<long long>::max()))return false;numbers.push_back(static_cast<long long>(*v));}
  else return false;
 }
 if(numbers.size()<2)return true;
 // pandas (maybe_sequence_to_range / is_sequence_range) works in wrapping
 // int64 arithmetic; unsigned differences reproduce it exactly everywhere,
 // where a long double difference was exact only on x86.
 const auto difference=[&](std::size_t i){return static_cast<unsigned long long>(numbers[i])-static_cast<unsigned long long>(numbers[i-1]);};
 const auto step=difference(1);if(step==0)return false;
 for(std::size_t i=2;i<numbers.size();++i)if(difference(i)!=step)return false;
 return true;
}
EventTable reorder(const EventTable& source,const std::vector<std::size_t>& order){
 EventTable result=source;result.rows.clear();if(result.index)result.index->labels.clear();else result.index=EventTableIndex{"RangeIndex","int64",{std::nullopt},{}};
 for(auto i:order){result.rows.push_back(source.rows[i]);
  if(source.index)result.index->labels.push_back(source.index->labels[i]);
  else{EventAttribute a{"",std::to_string(i),AttributeKind::kInt};a.typed_value=static_cast<std::uint64_t>(i);result.index->labels.push_back({std::move(a)});}
 }
 if(result.index->kind=="RangeIndex"&&!arithmetic(result.index->labels))result.index->kind="Index";
 return result;
}
EventAttribute subscript(const EventAttribute& value,std::size_t i){
 if(text_value(value)){
  std::size_t position=0;
  for(std::size_t n=0;position<value.value.size();++n){const auto start=position;const auto c=static_cast<unsigned char>(value.value[position]);position+=c<0x80?1:c<0xe0?2:c<0xf0?3:4;if(n==i)return EventAttribute{"",value.value.substr(start,position-start)};}
  error("IndexError","string index out of range");
 }
 if(value.container==AttributeContainerKind::kList||value.container==AttributeContainerKind::kTuple){
  if(i<value.children.size())return value.children[i];error("IndexError",type_name(value)+" index out of range");
 }
 error("TypeError","'"+type_name(value)+"' object is not subscriptable");
}
std::optional<std::string> encoded_pair(const EventAttribute& a,const EventAttribute& b){
 if(is_native_na_attribute(a)||is_native_na_attribute(b))return std::nullopt;
 if(!text_value(a)){
  if(a.kind==AttributeKind::kBytes)error("TypeError","can't concat str to bytes");
  if(a.container==AttributeContainerKind::kTuple||a.container==AttributeContainerKind::kList)error("TypeError","can only concatenate "+type_name(a)+" (not \"str\") to "+type_name(a));
  error("TypeError","unsupported operand type(s) for +: '"+type_name(a)+"' and 'str'");
 }
 if(!text_value(b))error("TypeError","can only concatenate str (not \""+type_name(b)+"\") to str");
 return a.value+","+b.value;
}
std::vector<std::optional<std::string>> query_paths(const EventAttribute& paths,bool performance){
 if(performance)return {encoded_pair(subscript(paths,0),subscript(paths,1))};
 std::vector<EventAttribute> values;
 if(text_value(paths)){for(std::size_t i=0;i<paths.value.size();++i)values.push_back(subscript(paths,i));}
 else if(paths.container!=AttributeContainerKind::kScalar)values=paths.children;
 else error("TypeError","'"+type_name(paths)+"' object is not iterable");
 std::vector<std::optional<std::string>> result;
 for(const auto& v:values)result.push_back(encoded_pair(subscript(v,0),subscript(v,1)));
 return result;
}
// One comparison of the occurrence durations with a bound. A list needs one
// item per occurrence and compares item by item. A tuple broadcasts against
// the durations, and the result is then assigned to the occurrences.
std::vector<bool> duration_mask(const std::vector<std::pair<std::size_t,double>>& occurrences,const EventAttribute& bound,bool lower){
 const auto n=occurrences.size();const std::string op=lower?"'>='":"'<='";
 if(text_value(bound))error("TypeError","Invalid comparison between dtype=float64 and str");
 if(date_value(bound)&&std::holds_alternative<NativeDate>(bound.typed_value))error("TypeError",op+" not supported between instances of 'numpy.ndarray' and '"+type_name(bound)+"'");
 const auto compare=[&](double value,const EventAttribute& item)->bool{
  if(item.is_null)return false;
  if(item.container!=AttributeContainerKind::kScalar||!numeric(item)||is_native_na_attribute(item))error("TypeError",op+" not supported between instances of 'float' and '"+type_name(item)+"'");
  const auto number=native_attribute_number(item);return lower?value>=number:value<=number;
 };
 std::vector<bool> result(n);
 if(bound.container==AttributeContainerKind::kList||bound.container==AttributeContainerKind::kTuple){
  const auto k=bound.children.size();
  if(bound.container==AttributeContainerKind::kList&&k!=n)error("ValueError","('Lengths must match to compare', ("+std::to_string(n)+",), ("+std::to_string(k)+",))");
  if(bound.container==AttributeContainerKind::kTuple){
   if(k!=n&&k!=1&&n!=1)error("ValueError","operands could not be broadcast together with shapes ("+std::to_string(n)+",) ("+std::to_string(k)+",) ");
   if(n==1&&k!=1)error("ValueError","Length of values ("+std::to_string(k)+") does not match length of index (1)");
  }
  for(std::size_t i=0;i<n;++i)result[i]=compare(occurrences[i].second,bound.children[k==n?i:0]);
  return result;
 }
 for(std::size_t i=0;i<n;++i)result[i]=compare(occurrences[i].second,bound);
 return result;
}
double elapsed(const EventAttribute& a,const EventAttribute& b){
 if(missing(a)||missing(b))return std::numeric_limits<double>::quiet_NaN();
 if(!date_value(a)||!date_value(b)){
  if(text_value(a)||text_value(b))error("TypeError","unsupported operand type(s) for -: '"+type_name(b)+"' and '"+type_name(a)+"'");
  return native_attribute_number(b)-native_attribute_number(a);
 }
 const auto x=instant(a),y=instant(b);if(!x||!y)return std::numeric_limits<double>::quiet_NaN();
 return static_cast<double>(y->seconds-x->seconds)+(static_cast<double>(y->nanoseconds)-x->nanoseconds)/1e9;
}
EventTable apply_paths(const EventTable& source,const EventAttribute& query,const PandasPathsFilterOptions& o,bool performance){
 const auto wanted=query_paths(query,performance);
 source.validate();const auto cases=sort_column(source,o.case_id_key),time=sort_column(source,o.timestamp_key);
 const auto order=sorted_rows(source,cases,time);const auto sorted=reorder(source,order);
 const auto activity=activity_column(sorted,o.attribute_key);
 const auto target=performance?activity:activity_column(sorted,o.target_attribute_key?*o.target_attribute_key:o.attribute_key);
 if(!sorted.dtypes.empty()&&sorted.dtypes[activity]=="category")error("TypeError","unsupported operand type(s) for +: 'Categorical' and 'str'");
 // The dataframe concatenation evaluates the selected attribute array before
 // membership, including ordinary unsupported nonstring scalar cells.
 for(std::size_t i=0;i<sorted.size();++i){const auto& a=cell(sorted,i,activity);if(!missing(a))(void)encoded_pair(a,EventAttribute{"",""});}
 std::vector<std::pair<std::size_t,double>> occurrences;
 std::unordered_set<std::string> selected;
 for(std::size_t i=0;i+1<sorted.size();++i){
  if(!same_case(cell(sorted,i,cases),cell(sorted,i+1,cases)))continue;
  const auto& a=cell(sorted,i,activity);const auto& b=cell(sorted,i+1,target);
  if(missing(a)||missing(b))continue;
  const auto path=encoded_pair(a,b);
  if(!path||std::none_of(wanted.begin(),wanted.end(),[&](const auto& w){return w&&*w==*path;}))continue;
  if(performance)occurrences.emplace_back(i,elapsed(cell(sorted,i,time),cell(sorted,i+1,time)));
  else selected.insert(group_key(cell(sorted,i,cases)));
 }
 if(performance){
  if(!sorted.dtypes.empty()&&!sorted.dtypes[time].starts_with("datetime64"))error("AttributeError","Can only use .dt accessor with datetimelike values");
  // Occurrences without a duration (NaT) are dropped before the bounds apply.
  std::erase_if(occurrences,[](const auto& o){return std::isnan(o.second);});
  const auto lower=duration_mask(occurrences,o.min_performance,true);
  const auto upper=duration_mask(occurrences,o.max_performance,false);
  for(std::size_t k=0;k<occurrences.size();++k)if(lower[k]&&upper[k])selected.insert(group_key(cell(sorted,occurrences[k].first,cases)));
 }
 const bool positive=truth(o.positive);std::vector<std::size_t> keep;
 for(std::size_t i=0;i<sorted.size();++i){const auto& value=cell(sorted,i,cases);const bool found=!missing(value)&&selected.contains(group_key(value));if(found==positive)keep.push_back(i);}
 return reorder(sorted,keep);
}
PandasPathsFilterOptions resolve_paths_options(const std::vector<PandasPathsFilterParameter>& params){
 PandasPathsFilterOptions o;
 const auto assign=[&](const EventAttribute& p){
  if(p.key=="pm4py:param:case_id_key")o.case_id_key=p;
  else if(p.key=="pm4py:param:attribute_key")o.attribute_key=p;
  else if(p.key=="target_attribute_key")o.target_attribute_key=p;
  else if(p.key=="pm4py:param:timestamp_key")o.timestamp_key=p;
  else if(p.key=="positive")o.positive=p;
  else if(p.key=="min_performance")o.min_performance=p;
  else if(p.key=="max_performance")o.max_performance=p;
 };
 for(const auto& p:params)if(!p.enum_key)assign(p.value);
 for(const auto& p:params)if(p.enum_key)assign(p.value);
 return o;
}
PandasPathsFilterOptions resolve_paths_options(const EventAttribute& params){
 std::vector<PandasPathsFilterParameter> values;
 if(params.container==AttributeContainerKind::kContainer){for(const auto& p:params.children)values.push_back({p,false});return resolve_paths_options(values);}
 if(params.is_null||(params.container==AttributeContainerKind::kList&&params.children.empty()))return {};
 detail::check_parameter_carrier(params,[](const std::string& type,const std::string& message){error(type,message);});
 return {};
}
} // namespace
EventTable filter_pandas_paths(const EventTable& s,const EventAttribute& paths,const PandasPathsFilterOptions& o){return apply_paths(s,paths,o,false);}
EventTable filter_pandas_path_performance(const EventTable& s,const EventAttribute& path,const PandasPathsFilterOptions& o){return apply_paths(s,path,o,true);}
EventTable filter_pandas_paths(const EventTable& s,const EventAttribute& paths,const EventAttribute& params){return apply_paths(s,paths,resolve_paths_options(params),false);}
EventTable filter_pandas_path_performance(const EventTable& s,const EventAttribute& path,const EventAttribute& params){return apply_paths(s,path,resolve_paths_options(params),true);}
EventTable filter_pandas_paths(const EventTable& s,const EventAttribute& paths,const std::vector<PandasPathsFilterParameter>& params){return apply_paths(s,paths,resolve_paths_options(params),false);}
EventTable filter_pandas_path_performance(const EventTable& s,const EventAttribute& path,const std::vector<PandasPathsFilterParameter>& params){return apply_paths(s,path,resolve_paths_options(params),true);}
} // namespace minera
