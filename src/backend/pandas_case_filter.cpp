#include "minera/pandas_case_filter.hpp"
#include "pandas_parameter_carrier.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "business_hours.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"
namespace minera {
namespace {
[[noreturn]] void error(std::string kind,std::string message){throw PandasCaseFilterError(std::move(kind),std::move(message));}
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
 if(date_value(v))return std::get_if<NativeDate>(&v.typed_value)&&std::get<NativeDate>(v.typed_value).resolution==NativeDateResolution::kNanosecond?"Timestamp":"datetime.datetime";
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
 const auto found=std::find(s.columns.begin(),s.columns.end(),key.value);
 if(found==s.columns.end())return std::nullopt;
 return static_cast<std::size_t>(found-s.columns.begin());
}
// pandas Index's string display escapes line controls but retains literal
// quote/NUL bytes; KeyError then applies Python repr to the complete message.
std::string index_item(const EventAttribute& key){
 if(text_value(key)){
  std::string result="'";
  for(const auto c:key.value){if(c=='\n')result+="\\n";else if(c=='\r')result+="\\r";else if(c=='\t')result+="\\t";else result+=c;}
  return result+"'";
 }
 if(key.container==AttributeContainerKind::kList&&key.children.empty())return "()";
 return repr(key);
}
std::string missing_columns(const std::vector<EventAttribute>& keys,bool all){
 std::string list="[";bool strings=true,integers=true;
 for(std::size_t i=0;i<keys.size();++i){if(i)list+=", ";list+=index_item(keys[i]);strings&=text_value(keys[i]);integers&=type_name(keys[i])=="int";}
 list+=']';
 if(all)return string_key_repr("None of [Index("+list+", dtype='"+(strings?"str":integers?"int64":"object")+"')] are in the [columns]");
 return string_key_repr(list+" not in index");
}
std::size_t size_column(const EventTable& s,const EventAttribute& key){
 if(key.container==AttributeContainerKind::kContainer)error("TypeError","unhashable type: 'dict'");
 if(const auto i=find_column(s,key))return *i;
 error("KeyError",missing_columns({key},true));
}
std::size_t ncases_column(const EventTable& s,const EventAttribute& key){
 if(key.container==AttributeContainerKind::kContainer)error("TypeError","Passing a dict as an indexer is not supported. Use a list instead.");
 if(key.container==AttributeContainerKind::kList&&key.children.empty())error("ValueError","No group keys passed!");
 if(const auto i=find_column(s,key))return *i;
 error("KeyError",repr(key));
}
std::pair<std::size_t,std::size_t> performance_columns(const EventTable& s,const PandasCaseFilterOptions& o){
 for(const auto* key:{&o.case_id_key,&o.timestamp_key})if(!root_cause_key_is_hashable(*key))error("TypeError","unhashable type: '"+type_name(*key)+"'");
 const auto c=find_column(s,o.case_id_key),t=find_column(s,o.timestamp_key);
 if(c&&t)return {*c,*t};
 if(!c&&!t)error("KeyError",missing_columns({o.case_id_key,o.timestamp_key},true));
 const auto& key=c?o.timestamp_key:o.case_id_key;
 const std::string item=key.is_null?"nan":repr(key);
 error("KeyError",string_key_repr("["+item+"] not in index"));
}
struct Group{std::string key;std::size_t count=0;const EventAttribute* first=nullptr;const EventAttribute* last=nullptr;};
struct Groups{std::vector<Group> values;std::unordered_map<std::string,std::size_t> positions;std::vector<std::optional<std::size_t>> rows;};
Groups groups(const EventTable& s,std::size_t c,std::optional<std::size_t> timestamp=std::nullopt){
 Groups result;result.rows.resize(s.size());
 for(std::size_t i=0;i<s.size();++i){
  const auto& cell=s.rows[i][c];if(!cell||missing(*cell))continue;
  const auto key=group_key(*cell);const auto [found,added]=result.positions.emplace(key,result.values.size());
  if(added)result.values.push_back(Group{key});
  result.rows[i]=found->second;auto& group=result.values[found->second];++group.count;
  if(timestamp){const auto& time=s.rows[i][*timestamp];if(time&&!missing(*time)){if(!group.first)group.first=&*time;group.last=&*time;}}
 }
 return result;
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
void validate_bound(const EventAttribute& bound,bool lower,bool duration){
 const std::string dtype=duration?"float64":"int64";
 if(text_value(bound))error("TypeError","Invalid comparison between dtype="+dtype+" and str");
 if(date_value(bound)&&std::holds_alternative<NativeDate>(bound.typed_value))error("TypeError",std::string(lower?"'>='":"'<='")+" not supported between instances of 'numpy.ndarray' and '"+type_name(bound)+"'");
 if(!bound.is_null&&!numeric(bound))error("TypeError",std::string(lower?"'>='":"'<='")+" not supported between instances of '"+(duration?"float":"int")+"' and '"+type_name(bound)+"'");
}
// One Series comparison of case durations with a bound: a str fails for any
// length, a list must have one item per case and compares item by item, a
// tuple broadcasts like a numpy operand, and other objects fail on the first
// case.
// The same rules hold for the int64 Series of per-row case sizes.
std::vector<bool> duration_mask(const std::vector<double>& durations,const std::vector<std::size_t>& cases,const EventAttribute& bound,bool lower,bool sizes=false){
 const auto n=cases.size();const std::string op=lower?"'>='":"'<='";const std::string item_type=sizes?"int":"float";
 if(text_value(bound))error("TypeError",std::string("Invalid comparison between dtype=")+(sizes?"int64":"float64")+" and str");
 if(date_value(bound)&&std::holds_alternative<NativeDate>(bound.typed_value))error("TypeError",op+" not supported between instances of 'numpy.ndarray' and '"+type_name(bound)+"'");
 const auto compare=[&](double value,const EventAttribute& item)->bool{
  if(item.is_null)return false;
  if(item.container!=AttributeContainerKind::kScalar||!numeric(item)||is_native_na_attribute(item))error("TypeError",op+" not supported between instances of '"+item_type+"' and '"+type_name(item)+"'");
  const auto number=native_attribute_number(item);return lower?value>=number:value<=number;
 };
 std::vector<bool> result(n);
 if(bound.container==AttributeContainerKind::kList||bound.container==AttributeContainerKind::kTuple){
  const auto k=bound.children.size();
  if(bound.container==AttributeContainerKind::kList&&k!=n)error("ValueError","('Lengths must match to compare', ("+std::to_string(n)+",), ("+std::to_string(k)+",))");
  if(bound.container==AttributeContainerKind::kTuple&&k!=n&&k!=1){
   // One case broadcasts to the tuple, whose result is assigned to that case.
   if(n==1)error("ValueError","Length of values ("+std::to_string(k)+") does not match length of index (1)");
   error("ValueError","operands could not be broadcast together with shapes ("+std::to_string(n)+",) ("+std::to_string(k)+",) ");
  }
  for(std::size_t i=0;i<n;++i)result[i]=compare(durations[cases[i]],bound.children[k==n?i:0]);
  return result;
 }
 for(std::size_t i=0;i<n;++i)result[i]=compare(durations[cases[i]],bound);
 return result;
}
bool accepts(double value,const EventAttribute& bound,bool lower){
 if(bound.is_null)return false;
 const auto n=native_attribute_number(bound);return lower?value>=n:value<=n;
}
std::vector<BusinessHourSlot> business_slots(const EventAttribute& value){
 if(value.is_null)error("TypeError","'NoneType' object is not iterable");
 if(text_value(value))error("ValueError","not enough values to unpack (expected 2, got 1)");
 if(value.container==AttributeContainerKind::kScalar)error("TypeError","'"+type_name(value)+"' object is not iterable");
 std::vector<BusinessHourSlot> result;
 for(const auto& pair:value.children){
  if(pair.children.size()<2)error("ValueError","not enough values to unpack (expected 2, got "+std::to_string(pair.children.size())+")");
  if(pair.children.size()>2)error("ValueError","too many values to unpack (expected 2)");
  if(!numeric(pair.children[0])||!numeric(pair.children[1]))error("TypeError","unsupported operand type(s) for -: '"+type_name(pair.children[1])+"' and '"+type_name(pair.children[0])+"'");
  result.push_back({native_attribute_number(pair.children[0]),native_attribute_number(pair.children[1])});
 }
 std::sort(result.begin(),result.end(),[](auto a,auto b){return a.start_seconds<b.start_seconds;});
 std::vector<BusinessHourSlot> merged;
 for(const auto& slot:result){if(!merged.empty()&&slot.start_seconds<=merged.back().end_seconds+1)merged.back().end_seconds=std::max(merged.back().end_seconds,slot.end_seconds);else merged.push_back(slot);}
 return merged;
}
double duration(const Group& g,bool business,const std::vector<BusinessHourSlot>& slots,std::int64_t unit_nanoseconds=1){
 if(!g.first||!g.last){if(business)error("ValueError","cannot convert float NaN to integer");return std::numeric_limits<double>::quiet_NaN();}
 if(!date_value(*g.first)||!date_value(*g.last)){
  if(text_value(*g.first)||text_value(*g.last))error("TypeError","unsupported operand type(s) for -: '"+type_name(*g.last)+"' and '"+type_name(*g.first)+"'");
  error("AttributeError","Can only use .dt accessor with datetimelike values");
 }
 if(business){
  const auto* a=std::get_if<NativeDate>(&g.first->typed_value);const auto* b=std::get_if<NativeDate>(&g.last->typed_value);
  const auto start=detail::local_wall_timestamp_microseconds(a?a->value:g.first->value),end=detail::local_wall_timestamp_microseconds(b?b->value:g.last->value);
  if(!start||!end)error("ValueError","cannot convert float NaN to integer");
  return detail::business_elapsed_seconds_microseconds(*start,*end,slots);
 }
 const auto a=instant(*g.first),b=instant(*g.last);if(!a||!b)return std::numeric_limits<double>::quiet_NaN();
 // Timedelta.total_seconds: the exact integer count of the column's units,
 // converted once and divided by the units per second. Subtracting rounded
 // seconds would cancel digits near second boundaries.
 const std::int64_t per_second=1000000000/unit_nanoseconds,seconds=b->seconds-a->seconds;
 const std::int64_t fraction=(static_cast<std::int64_t>(b->nanoseconds)-a->nanoseconds)/unit_nanoseconds;
 if(seconds>std::numeric_limits<std::int64_t>::max()/per_second-1||seconds<std::numeric_limits<std::int64_t>::min()/per_second+1)
  return static_cast<double>(seconds)+static_cast<double>(fraction)/static_cast<double>(per_second);
 return static_cast<double>(seconds*per_second+fraction)/static_cast<double>(per_second);
}
} // namespace

PandasCaseFilterOptions::PandasCaseFilterOptions(){
 business_hours.typed_value=false;business_hour_slots.container=AttributeContainerKind::kList;
 for(std::int64_t day=0;day<5;++day){EventAttribute slot{"",""};slot.container=AttributeContainerKind::kTuple;
  for(const auto offset:{25200LL,61200LL}){EventAttribute v{"",std::to_string(day*86400+offset),AttributeKind::kInt};v.typed_value=std::int64_t(day*86400+offset);slot.children.push_back(std::move(v));}
  business_hour_slots.children.push_back(std::move(slot));
 }
}
EventTable filter_pandas_case_size(const EventTable& s,const EventAttribute& minimum,const EventAttribute& maximum,const PandasCaseFilterOptions& o){
 s.validate();const auto c=size_column(s,o.case_id_key);const auto grouped=groups(s,c);
 if(s.size()&&grouped.values.empty())error("IndexError","cannot do a non-empty take from an empty axes.");
 // Each row carries its case size. A list or tuple bound compares with the
 // rows item by item (a list needs one item per row; a tuple broadcasts).
 std::vector<double> counts(s.size());std::vector<std::size_t> rows(s.size());std::iota(rows.begin(),rows.end(),0);
 for(std::size_t i=0;i<s.size();++i)counts[i]=grouped.rows[i]?static_cast<double>(grouped.values[*grouped.rows[i]].count):std::numeric_limits<double>::quiet_NaN();
 const auto vector_bound=[](const EventAttribute& bound){return bound.container==AttributeContainerKind::kList||bound.container==AttributeContainerKind::kTuple;};
 std::vector<bool> lower(s.size());
 if(vector_bound(minimum))lower=duration_mask(counts,rows,minimum,true,true);
 else{validate_bound(minimum,true,false);for(std::size_t i=0;i<s.size();++i)lower[i]=accepts(counts[i],minimum,true);}
 auto result=subset(s,lower);
 if(maximum.is_null)return result;
 // The upper mask is computed on every row and aligned to the kept rows by
 // label: a duplicated label takes every row carrying it, then the mask is
 // applied by position.
 std::vector<bool> upper(s.size());
 if(vector_bound(maximum))upper=duration_mask(counts,rows,maximum,false,true);
 else{validate_bound(maximum,false,false);for(std::size_t i=0;i<s.size();++i)upper[i]=accepts(counts[i],maximum,false);}
 const auto labels=[](const EventTable& t){std::vector<EventTableIndexLabel> r;for(std::size_t i=0;i<t.size();++i){if(t.index)r.push_back(t.index->labels[i]);else {EventAttribute label{"",std::to_string(i),AttributeKind::kInt};label.typed_value=static_cast<std::uint64_t>(i);r.push_back({std::move(label)});}}return r;};
 const auto all=labels(s),kept=labels(result);
 std::vector<bool> aligned;
 if(all==kept)aligned=upper;
 else for(const auto& label:kept)for(std::size_t i=0;i<all.size();++i)if(all[i]==label)aligned.push_back(upper[i]);
 if(std::all_of(aligned.begin(),aligned.end(),[](bool v){return v;}))return result;
 std::vector<bool> keep(result.size());
 for(std::size_t i=0;i<aligned.size();++i)if(aligned[i]){if(i>=result.size())error("IndexError","indices are out-of-bounds");keep[i]=true;}
 return subset(result,keep);
}
EventTable filter_pandas_ncases(const EventTable& s,const EventAttribute& maximum,const PandasCaseFilterOptions& o){
 s.validate();const auto c=ncases_column(s,o.case_id_key);const auto grouped=groups(s,c);
 if(maximum.is_null||!numeric(maximum))error("TypeError","'<' not supported between instances of '"+type_name(maximum)+"' and 'int'");
 const auto number=native_attribute_number(maximum);std::size_t count=grouped.values.size();
 if(number<static_cast<double>(count)){
  if(type_name(maximum)=="float")error("TypeError","slice indices must be integers or None or have an __index__ method");
  if(number<0)count=number<=-static_cast<double>(count)?0:count-static_cast<std::size_t>(-number);
  else count=static_cast<std::size_t>(number);
 }
 std::vector<std::size_t> order(grouped.values.size());std::iota(order.begin(),order.end(),0);
 std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return grouped.values[a].count>grouped.values[b].count;});
 std::vector<bool> accepted(grouped.values.size());for(std::size_t i=0;i<count;++i)accepted[order[i]]=true;
 std::vector<bool> keep(s.size());for(std::size_t i=0;i<s.size();++i)keep[i]=grouped.rows[i]&&accepted[*grouped.rows[i]];
 return subset(s,keep);
}
EventTable filter_pandas_case_performance(const EventTable& s,const EventAttribute& minimum,const EventAttribute& maximum,const PandasCaseFilterOptions& o){
 s.validate();const auto [c,t]=performance_columns(s,o);const auto grouped=groups(s,c,t);const bool business=truth(o.business_hours);
 if(business&&grouped.values.empty())error("ValueError","Cannot set a DataFrame with multiple columns to the single column caseDuration");
 const auto slots=business?business_slots(o.business_hour_slots):std::vector<BusinessHourSlot>{};
 // The datetime column's resolution (ns by default) sets the Timedelta unit.
 std::int64_t unit=1;
 if(!s.dtypes.empty()){const auto& dtype=s.dtypes[t];
  if(dtype.starts_with("datetime64[us"))unit=1000;else if(dtype.starts_with("datetime64[ms"))unit=1000000;else if(dtype.starts_with("datetime64[s"))unit=1000000000;}
 std::vector<double> durations;for(const auto& g:grouped.values)durations.push_back(duration(g,business,slots,unit));
 if(!business&&!s.dtypes.empty()&&!s.dtypes[t].starts_with("datetime64"))error("AttributeError","Can only use .dt accessor with datetimelike values");
 // Durations are a Series over the cases in sorted case order. The maximum
 // is applied first, then the minimum to the cases that remain.
 std::vector<std::size_t> order(grouped.values.size());std::iota(order.begin(),order.end(),0);
 {std::vector<const EventAttribute*> ids(grouped.values.size());for(std::size_t i=0;i<s.size();++i)if(grouped.rows[i]&&!ids[*grouped.rows[i]])ids[*grouped.rows[i]]=&*s.rows[i][c];
  // pandas sorts group keys; keys that cannot be compared (such as 1 and
  // 'a') fall back to its mixed order: numbers, then strings.
  std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){
   try{return native_scalar_key_compare(*ids[a],*ids[b])<0;}
   catch(const std::invalid_argument&){return !text_value(*ids[a])&&text_value(*ids[b]);}
  });}
 std::vector<bool> accepted(grouped.values.size());
 std::vector<std::size_t> remaining;
 const auto upper=duration_mask(durations,order,maximum,false);
 for(std::size_t i=0;i<order.size();++i)if(upper[i])remaining.push_back(order[i]);
 const auto lower=duration_mask(durations,remaining,minimum,true);
 for(std::size_t i=0;i<remaining.size();++i)if(lower[i])accepted[remaining[i]]=true;
 std::vector<bool> keep(s.size());for(std::size_t i=0;i<s.size();++i)keep[i]=grouped.rows[i]&&accepted[*grouped.rows[i]];
 return subset(s,keep);
}
EventTable filter_pandas_case_performance(const EventTable& s,const EventAttribute& minimum,const EventAttribute& maximum,const EventAttribute& parameters){
 PandasCaseFilterOptions options;
 if(!parameters.is_null&&parameters.container!=AttributeContainerKind::kContainer)
  detail::check_parameter_carrier(parameters,[](const std::string& type,const std::string& message){error(type,message);});
 if(parameters.container==AttributeContainerKind::kContainer)for(const auto& p:parameters.children){
  if(p.key=="pm4py:param:case_id_key")options.case_id_key=p;
  else if(p.key=="pm4py:param:timestamp_key")options.timestamp_key=p;
  else if(p.key=="business_hours")options.business_hours=p;
  else if(p.key=="business_hour_slots")options.business_hour_slots=p;
 }
 return filter_pandas_case_performance(s,minimum,maximum,options);
}
EventTable apply_pandas_case_filter(const EventTable&,const EventAttribute&){error("NotImplementedError","apply method not available for case filter");}
EventTable auto_filter_pandas_cases(const EventTable&,const EventAttribute&){error("Exception","apply_auto_filter method not available for case filter");}
} // namespace minera
