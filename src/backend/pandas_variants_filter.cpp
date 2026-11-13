#include "minera/pandas_variants_filter.hpp"
#include "pandas_parameter_carrier.hpp"
#include <algorithm>
#include <functional>
#include <numeric>
#include <cmath>
#include <limits>
#include <optional>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "minera/timestamp.hpp"
#include "numpy_object_sort.hpp"
#include "python_list_sort.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"
namespace minera { namespace {
[[noreturn]] void error(std::string kind,std::string message){throw PandasVariantsFilterError(std::move(kind),std::move(message));}
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
    else error("ValueError", "pandas variants selector has invalid UTF-8");
    for (unsigned i = 0; i < continuation; ++i) {
      if (position == value.size()) error("ValueError", "pandas variants selector has short UTF-8");
      const auto next = static_cast<unsigned char>(value[position++]);
      if ((next & 0xc0) != 0x80) error("ValueError", "pandas variants selector has invalid UTF-8 continuation");
      code = (code << 6) | (next & 63);
    }
    if ((continuation == 1 && code < 0x80) ||
        (continuation == 2 && code < 0x800) ||
        (continuation == 3 && code < 0x10000) || code > 0x10ffff)
      error("ValueError", "pandas variants selector has invalid code point");
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

enum class Mode { kMembership, kTopK, kMinimum, kMaximum };
std::optional<std::size_t> column(const EventTable& source,const EventAttribute& key){
 if(!text_value(key))return std::nullopt;
 const auto found=std::find(source.columns.begin(),source.columns.end(),key.value);
 return found==source.columns.end()?std::nullopt:std::optional<std::size_t>(found-source.columns.begin());
}
std::size_t direct_column(const EventTable& source,const EventAttribute& key){
 if(const auto found=column(source,key))return *found;
 error("KeyError",repr(key));
}
// A missing cell reads as its column's missing value (round BY): NaT in a
// datetime64 column, NaN in a NumPy float column, None otherwise.
const EventAttribute& cell(const EventTable& source,std::size_t row,std::size_t col){
 static const EventAttribute none{"","",AttributeKind::kString,true};
 static const EventAttribute nat{"","NaT",AttributeKind::kDate,true};
 static const EventAttribute nan=[]{EventAttribute v{"","nan",AttributeKind::kFloat};v.typed_value=std::numeric_limits<double>::quiet_NaN();return v;}();
 if(source.rows[row][col])return *source.rows[row][col];
 if(col<source.dtypes.size()){
  const auto& dtype=source.dtypes[col];
  if(dtype.starts_with("datetime64["))return nat;
  if(dtype=="float16"||dtype=="float32"||dtype=="float64")return nan;
 }
 return none;
}
// How NumPy holds a column: typed numbers (bool, ints), typed floats, a naive
// datetime64 array, or Python objects (object columns, aware datetimes as
// Timestamps, extension dtypes).
enum class ArrayKind { kObject, kNumber, kFloat, kDatetime };
ArrayKind array_kind(const EventTable& source,std::size_t col){
 if(col>=source.dtypes.size())return ArrayKind::kObject;
 const auto& dtype=source.dtypes[col];
 if(dtype=="float16"||dtype=="float32"||dtype=="float64")return ArrayKind::kFloat;
 if(dtype=="bool"||((dtype.starts_with("int")||dtype.starts_with("uint"))&&dtype.find_first_not_of("uint0123456789")==std::string::npos))return ArrayKind::kNumber;
 if(dtype.starts_with("datetime64[")&&dtype.find(',')==std::string::npos)return ArrayKind::kDatetime;
 return ArrayKind::kObject;
}
// NumPy float and naive datetime64 arrays hand out a fresh NaN or NaT object
// per item, which equals nothing, not even itself; None, pd.NaT and an object
// column's NaN are single objects that equal themselves.
bool fresh_missing(ArrayKind kind){return kind==ArrayKind::kFloat||kind==ArrayKind::kDatetime;}
// NaN and NaT compare False both ways.
bool unordered(const EventAttribute& value){return !is_native_na_attribute(value)&&value.container==AttributeContainerKind::kScalar&&((value.is_null&&date_value(value))||(!value.is_null&&numeric(value)&&std::isnan(native_attribute_number(value))));}
bool holds_missing(const EventAttribute& variant){return std::any_of(variant.children.begin(),variant.children.end(),[](const EventAttribute& v){return unordered(v);});}
EventAttribute tuple(std::vector<EventAttribute> children){
 EventAttribute result{"",""};result.container=AttributeContainerKind::kTuple;result.children=std::move(children);return result;
}
int compare_date(const EventAttribute& left,const EventAttribute& right){
 const auto a=instant(left),b=instant(right);
 if(!a||!b)return 0;
 if(a->seconds!=b->seconds)return a->seconds<b->seconds?-1:1;
 return a->nanoseconds<b->nanoseconds?-1:a->nanoseconds>b->nanoseconds?1:0;
}
int order(const EventAttribute& left,const EventAttribute& right){
 if(identity(left)==identity(right))return 0;
 if(is_native_na_attribute(left)||is_native_na_attribute(right))error("TypeError","boolean value of NA is ambiguous");
 if(left.container==AttributeContainerKind::kTuple&&right.container==AttributeContainerKind::kTuple){
  for(std::size_t i=0;i<std::min(left.children.size(),right.children.size());++i){const auto result=order(left.children[i],right.children[i]);if(result)return result;}
  return left.children.size()<right.children.size()?-1:left.children.size()>right.children.size()?1:0;
 }
 const auto a=native_scalar_order_kind(left),b=native_scalar_order_kind(right);
 if(!left.is_null&&!right.is_null&&a!=NativeScalarOrderKind::kUnsupported&&a==b){
  if(a==NativeScalarOrderKind::kDate)return compare_date(left,right);
  if(a==NativeScalarOrderKind::kNumeric&&(missing(left)||missing(right)))return 0;
  return native_scalar_key_compare(left,right);
 }
 error("TypeError","'<' not supported between instances of '"+type_name(left)+"' and '"+type_name(right)+"'");
}
struct Group {std::string key;EventAttribute id;std::size_t first=0;std::vector<std::size_t> rows;};
std::vector<Group> groups(const EventTable& source,std::size_t cases){
 std::vector<Group> result;std::unordered_map<std::string,std::size_t> positions;
 for(std::size_t i=0;i<source.size();++i){
  const auto& value=cell(source,i,cases);if(missing(value))continue;
  const auto key=group_key(value);const auto [position,added]=positions.emplace(key,result.size());
  if(added)result.push_back({key,value,i,{}});
  result[position->second].rows.push_back(i);
 }
 return result;
}
struct CacheRow {EventAttribute id;EventAttribute variant;};
std::vector<CacheRow> membership_cache(const EventTable& source,const PandasVariantsFilterOptions& options){
 if(options.variants_df){
  const auto* table=std::get_if<EventTable>(&*options.variants_df);
  if(!table){const auto& value=std::get<EventAttribute>(*options.variants_df);error("AttributeError","'"+type_name(value)+"' object has no attribute 'empty'");}
  table->validate();if(table->empty()||table->columns.empty())return {};
  const auto variant=direct_column(*table,EventAttribute{"","variant"});std::vector<CacheRow> result;
  for(std::size_t i=0;i<table->size();++i){
   EventAttribute id{"",std::to_string(i),AttributeKind::kInt};id.typed_value=static_cast<std::int64_t>(i);
   if(table->index){
    const auto& label=table->index->labels[i];
    if(const auto* scalar=std::get_if<EventAttribute>(&label.value))id=*scalar;
    else {std::vector<EventAttribute> parts;for(const auto& part:std::get<std::vector<EventTableIndexLabel>>(label.value)){
      if(const auto* scalar=std::get_if<EventAttribute>(&part.value))parts.push_back(*scalar);
      else error("TypeError","nested precomputed index tuples exceed the captured domain");
     }id=tuple(std::move(parts));}
   }
   result.push_back({std::move(id),cell(*table,i,variant)});
  }
  return result;
 }
 if(options.case_id_key.is_null)error("TypeError","You have to supply one of 'by' and 'level'");
 if(options.case_id_key.container==AttributeContainerKind::kList&&options.case_id_key.children.empty())error("ValueError","No group keys passed!");
 const auto cases=direct_column(source,options.case_id_key);
 if(options.activity_key.container==AttributeContainerKind::kList&&options.activity_key.children.empty())error("AttributeError","'DataFrame' object has no attribute 'to_frame'");
 const auto activity=column(source,options.activity_key);
 if(!activity)error("KeyError",string_key_repr("Column not found: "+(text_value(options.activity_key)?options.activity_key.value:repr(options.activity_key))));
 std::vector<CacheRow> result;
 for(const auto& group:groups(source,cases)){
  std::vector<EventAttribute> values;for(auto i:group.rows)values.push_back(cell(source,i,*activity));
  result.push_back({group.id,tuple(std::move(values))});
 }
 return result;
}
std::vector<EventAttribute> admitted_values(const EventAttribute& admitted){
 if(admitted.container==AttributeContainerKind::kContainer){
  std::vector<EventAttribute> result;for(const auto& value:admitted.children)result.push_back(EventAttribute{"",value.key});return result;
 }
 if(admitted.container!=AttributeContainerKind::kScalar)return admitted.children;
 error("TypeError","only list-like objects are allowed to be passed to isin(), you passed a `"+type_name(admitted)+"`");
}
// A naive datetime64[ns] cell with a sub-microsecond part.
bool nanosecond_date(const EventAttribute& value){
 if(value.is_null||!date_value(value))return false;
 const auto parsed=instant(value);return parsed&&parsed->nanoseconds%1000!=0;
}
EventTable membership(const EventTable& source,const PandasVariantsFilterOptions& options,const EventAttribute* criterion,const std::vector<EventAttribute>* supplied=nullptr,bool from_statistics=false){
 const auto cache=membership_cache(source,options);
 std::vector<bool> keep(source.size());
 // Without a non-null case nothing is selected; a negative filter keeps all.
 if(cache.empty()){std::fill(keep.begin(),keep.end(),!truth(options.positive));return subset(source,keep);}
 const auto admitted=supplied?*supplied:admitted_values(*criterion);
 std::unordered_set<std::string> variants;for(const auto& value:admitted)variants.insert(identity(value));
 // A variant holding a fresh NaN or NaT equals no admitted variant (round BY).
 const auto activity=options.variants_df?std::nullopt:column(source,options.activity_key);
 const bool fresh=activity&&fresh_missing(array_kind(source,*activity));
 // The statistics hold numpy datetime64 items and the cases Timestamps; with
 // a sub-microsecond part the two hash apart and never match (round BY).
 const bool split_hash=from_statistics&&activity&&*activity<source.dtypes.size()&&source.dtypes[*activity]=="datetime64[ns]";
 const auto unmatched=[&](const EventAttribute& variant){
  return (fresh&&holds_missing(variant))||(split_hash&&std::any_of(variant.children.begin(),variant.children.end(),nanosecond_date));
 };
 std::unordered_set<std::string> selected;for(const auto& row:cache)if(!unmatched(row.variant)&&variants.contains(identity(row.variant)))selected.insert(identity(row.id));
 const auto cases=column(source,options.case_id_key);
 if(!cases)error("KeyError",string_key_repr("None of ["+repr(options.case_id_key)+"] are in the columns"));
 const bool positive=truth(options.positive);
 for(std::size_t i=0;i<source.size();++i)keep[i]=selected.contains(identity(cell(source,i,*cases)))==positive;
 return subset(source,keep);
}
struct VariantCount {EventAttribute variant;std::size_t count=0;};
struct Statistics {std::vector<VariantCount> variants;std::size_t cases=0;ArrayKind activity_kind=ArrayKind::kObject;};
// Python's x == y of two items, identity first.
bool items_equal(const EventAttribute& x,const EventAttribute& y,bool fresh){
 if(unordered(x)||unordered(y))return !fresh&&identity(x)==identity(y);
 if(identity(x)==identity(y))return true;
 if(x.is_null||y.is_null||text_value(x)!=text_value(y))return false;
 try{return order(x,y)==0;}catch(const std::exception&){return false;}
}
// Python's x < y of two variant tuples: the first position whose items are
// not equal decides; NaN and NaT are never less nor greater (round BY).
bool variant_less(const EventAttribute& x,const EventAttribute& y,bool fresh){
 const auto n=std::min(x.children.size(),y.children.size());
 for(std::size_t i=0;i<n;++i){
  const auto& a=x.children[i];const auto& b=y.children[i];
  if(items_equal(a,b,fresh))continue;
  // Among numbers (or among dates) NaN and NaT compare False; against
  // another type Python raises first.
  const bool comparable=(date_value(a)&&date_value(b))||(numeric(a)&&numeric(b)&&!date_value(a)&&!date_value(b));
  if((unordered(a)||unordered(b))&&comparable)return false;
  return order(a,b)<0;
 }
 return x.children.size()<y.children.size();
}
// The case column as np.unique(return_index=True, return_counts=True) reads
// it (round BY): a stable sort, runs of equal neighbours, each run's first
// position and size. Typed arrays sort NaN and NaT last and gather them in
// one run; object arrays sort with numpy's object timsort and Python's
// comparisons (None and mixed types raise, NaN and NaT are unordered and
// each stays a run of its own).
struct CaseRun {std::size_t first=0,size=0;};
std::vector<CaseRun> case_runs(const EventTable& source,std::size_t cases){
 const auto kind=array_kind(source,cases);const auto n=source.size();
 std::vector<std::size_t> sorted;std::function<bool(std::size_t,std::size_t)> same;
 if(kind!=ArrayKind::kObject){
  const auto less=[&](std::size_t a,std::size_t b){const auto& x=cell(source,a,cases);const auto& y=cell(source,b,cases);
   if(unordered(x))return false;if(unordered(y))return true;return order(x,y)<0;};
  sorted.resize(n);std::iota(sorted.begin(),sorted.end(),std::size_t{0});std::stable_sort(sorted.begin(),sorted.end(),less);
  same=[&,less](std::size_t a,std::size_t b){return unordered(cell(source,a,cases))==unordered(cell(source,b,cases))&&!less(a,b)&&!less(b,a);};
 }else{
  sorted=detail::numpy_object_stable_argsort(n,[&](std::size_t a,std::size_t b)->int{
   const auto& x=cell(source,a,cases);const auto& y=cell(source,b,cases);
   if(is_native_na_attribute(x)||is_native_na_attribute(y))error("TypeError","boolean value of NA is ambiguous");
   const bool none_x=x.is_null&&!date_value(x),none_y=y.is_null&&!date_value(y);
   if(none_x||none_y||text_value(x)!=text_value(y))error("TypeError","'<' not supported between instances of '"+type_name(x)+"' and '"+type_name(y)+"'");
   if(unordered(x)||unordered(y))return 0;
   return order(x,y);
  });
  same=[&](std::size_t a,std::size_t b){const auto& x=cell(source,a,cases);const auto& y=cell(source,b,cases);
   if(x.is_null&&!date_value(x)&&y.is_null&&!date_value(y))return true;
   return !unordered(x)&&!unordered(y)&&items_equal(x,y,true);};
 }
 std::vector<CaseRun> runs;
 for(std::size_t at=0;at<sorted.size();){
  auto end=at+1;while(end<sorted.size()&&same(sorted[end-1],sorted[end]))++end;
  runs.push_back({sorted[at],end-at});at=end;
 }
 return runs;
}
Statistics statistics(const EventTable& source,const PandasVariantsFilterOptions& options){
 if(options.case_id_key.container==AttributeContainerKind::kList&&options.case_id_key.children.empty())error("ValueError","No group keys passed!");
 const auto cases=direct_column(source,options.case_id_key);
 (void)direct_column(source,options.timestamp_key);
 if(options.activity_key.container==AttributeContainerKind::kList&&options.activity_key.children.empty())error("TypeError","unhashable type: 'numpy.ndarray'");
 const auto activity=direct_column(source,options.activity_key);
 // The coverage counts the non-missing case ids (a groupby, which also
 // refuses unhashable case cells first), while the variants count every
 // np.unique run (round BY).
 const auto case_count=groups(source,cases).size();
 const auto runs=case_runs(source,cases);
 Statistics result;result.cases=case_count;result.activity_kind=array_kind(source,activity);std::unordered_map<std::string,std::size_t> positions;
 const bool fresh=fresh_missing(result.activity_kind);
 for(std::size_t r=0;r<runs.size();++r){
  // Public witnesses confirm statistics slice from the first position for the
  // total case size, while membership aggregates the actual case rows.
  std::vector<EventAttribute> values;
  for(std::size_t i=runs[r].first;i<std::min(source.size(),runs[r].first+runs[r].size);++i){
   const auto& value=cell(source,i,activity);if(!root_cause_key_is_hashable(value))error("TypeError","unhashable type: '"+type_name(value)+"'");values.push_back(value);
  }
  auto variant=tuple(std::move(values));
  // A tuple holding a fresh NaN or NaT is a dictionary key of its own.
  const auto key=fresh&&holds_missing(variant)?"fresh:"+std::to_string(r):identity(variant);
  const auto [position,added]=positions.emplace(key,result.variants.size());
  if(added)result.variants.push_back({std::move(variant),0});
  ++result.variants[position->second].count;
 }
 return result;
}
bool integer(const EventAttribute& value){return value.kind==AttributeKind::kInt||value.kind==AttributeKind::kBoolean||std::holds_alternative<std::int64_t>(value.typed_value)||std::holds_alternative<std::uint64_t>(value.typed_value)||std::holds_alternative<bool>(value.typed_value);}
TokenInteger exact_integer(const EventAttribute& value){
 if(value.kind==AttributeKind::kBoolean||std::holds_alternative<bool>(value.typed_value))return TokenInteger(truth(value)?1:0);
 if(const auto* stored=std::get_if<std::int64_t>(&value.typed_value))return TokenInteger(*stored);
 if(const auto* stored=std::get_if<std::uint64_t>(&value.typed_value))return TokenInteger(*stored);
 return TokenInteger(value.value);
}
std::size_t top_count(const EventAttribute& value,std::size_t count){
 if(is_native_na_attribute(value))error("TypeError","boolean value of NA is ambiguous");
 if(!numeric(value)||value.is_null)error("TypeError","'<' not supported between instances of 'int' and '"+type_name(value)+"'");
 if(integer(value)){
  auto selected=exact_integer(value);const auto total=TokenInteger(count);
  if(selected>total)selected=total;
  if(selected<0)selected+=total;
  if(selected<0)return 0;
  return static_cast<std::size_t>(selected);
 }
 if(native_attribute_number(value)>static_cast<double>(count))return count;
 error("TypeError","slice indices must be integers or None or have an __index__ method");
}
EventTable apply_filter(const EventTable& source,const EventAttribute& criterion,const PandasVariantsFilterOptions& options,Mode mode){
 source.validate();if(mode==Mode::kMembership)return membership(source,options,&criterion);
 auto stats=statistics(source,options);std::vector<EventAttribute> admitted;
 if(mode==Mode::kTopK){
  // Sorted by (count, variant) with reverse=True, as Python orders it, before
  // k is read.
  const bool fresh=fresh_missing(stats.activity_kind);
  detail::python_list_sort(stats.variants,[fresh](const VariantCount& a,const VariantCount& b){return a.count!=b.count?a.count<b.count:variant_less(a.variant,b.variant,fresh);},true);
  const auto selected=top_count(criterion,stats.variants.size());
  for(std::size_t i=0;i<selected;++i)admitted.push_back(stats.variants[i].variant);
 }else{
  if(!stats.variants.empty()){
   if(is_native_na_attribute(criterion))error("TypeError","boolean value of NA is ambiguous");
   if(criterion.is_null||criterion.container==AttributeContainerKind::kContainer||date_value(criterion))error("TypeError","unsupported operand type(s) for *: '"+type_name(criterion)+"' and 'int'");
   if(!numeric(criterion))error("TypeError",std::string(mode==Mode::kMinimum?"'>='":"'<='")+" not supported between instances of 'int' and '"+type_name(criterion)+"'");
   for(const auto& variant:stats.variants){
    bool keep;
    if(integer(criterion)){
     const auto threshold=exact_integer(criterion)*TokenInteger(stats.cases),count=TokenInteger(variant.count);
     keep=mode==Mode::kMinimum?count>=threshold:count<=threshold;
    }else {const auto threshold=native_attribute_number(criterion)*static_cast<double>(stats.cases);keep=mode==Mode::kMinimum?static_cast<double>(variant.count)>=threshold:static_cast<double>(variant.count)<=threshold;}
    if(keep)admitted.push_back(variant.variant);
   }
  }
 }
 return membership(source,options,nullptr,&admitted,true);
}
PandasVariantsFilterOptions resolve_variants_options(const std::vector<PandasVariantsFilterParameter>& parameters){
 PandasVariantsFilterOptions options;
 // Public witnesses retain the last normalized enum/string key in insertion order.
 for(const auto& p:parameters){const auto& value=p.value;
  if(value.key=="pm4py:param:case_id_key")options.case_id_key=value;
  else if(value.key=="pm4py:param:activity_key")options.activity_key=value;
  else if(value.key=="pm4py:param:timestamp_key"&&!p.enum_key)options.timestamp_key=value;
  else if(value.key=="positive")options.positive=value;
  else if(value.key=="decreasingFactor")options.decreasing_factor=value;
  else if(value.key=="variants_df"){if(p.table)options.variants_df=*p.table;else options.variants_df=value;}
 }
 return options;
}
PandasVariantsFilterOptions resolve_variants_options(const EventAttribute& parameters){
 if(parameters.is_null||(parameters.container==AttributeContainerKind::kList&&parameters.children.empty()))return {};
 if(parameters.container==AttributeContainerKind::kContainer){std::vector<PandasVariantsFilterParameter> values;for(const auto& value:parameters.children)values.push_back({value,false,std::nullopt});return resolve_variants_options(values);}
 detail::check_parameter_carrier(parameters,[](const std::string& type,const std::string& message){error(type,message);});
 return {};
}
} // namespace
#define MINERA_VARIANTS_FUNCTION(name,mode) \
EventTable name(const EventTable& source,const EventAttribute& criterion,const PandasVariantsFilterOptions& options){return apply_filter(source,criterion,options,Mode::mode);} \
EventTable name(const EventTable& source,const EventAttribute& criterion,const EventAttribute& parameters){return name(source,criterion,resolve_variants_options(parameters));} \
EventTable name(const EventTable& source,const EventAttribute& criterion,const std::vector<PandasVariantsFilterParameter>& parameters){return name(source,criterion,resolve_variants_options(parameters));}
MINERA_VARIANTS_FUNCTION(filter_pandas_variants,kMembership)
MINERA_VARIANTS_FUNCTION(filter_pandas_variants_top_k,kTopK)
MINERA_VARIANTS_FUNCTION(filter_pandas_variants_min_coverage,kMinimum)
MINERA_VARIANTS_FUNCTION(filter_pandas_variants_max_coverage,kMaximum)
#undef MINERA_VARIANTS_FUNCTION
EventTable filter_pandas_variants(const EventTable& source,const PandasVariantsAdmittedMapping& admitted,const PandasVariantsFilterOptions& options){
 source.validate();std::vector<EventAttribute> keys;for(const auto& entry:admitted.entries)keys.push_back(entry.first);return membership(source,options,nullptr,&keys);
}
EventTable filter_pandas_variants(const EventTable& source,const PandasVariantsAdmittedMapping& admitted,const EventAttribute& parameters){return filter_pandas_variants(source,admitted,resolve_variants_options(parameters));}
EventTable filter_pandas_variants(const EventTable& source,const PandasVariantsAdmittedMapping& admitted,const std::vector<PandasVariantsFilterParameter>& parameters){return filter_pandas_variants(source,admitted,resolve_variants_options(parameters));}
} // namespace minera
