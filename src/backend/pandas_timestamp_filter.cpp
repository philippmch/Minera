#include "minera/pandas_timestamp_filter.hpp"
#include "pandas_datetime_target.hpp"
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
[[noreturn]] void error(std::string kind,std::string message){throw PandasTimestampFilterError(std::move(kind),std::move(message));}
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

enum class Mode { kEvents, kContained, kIntersecting, kStarting, kCompleting, kAttribute };
std::optional<ParsedExactTimestamp> bound(const EventAttribute& value){
 if(date_value(value)){
  if(value.is_null)return std::nullopt;
  const auto* stored=std::get_if<NativeDate>(&value.typed_value);const auto& text=stored?stored->value:value.value;
  const auto zone=text.find_first_of("Zz+-",10);
  auto parsed=parse_timestamp_exact(text.substr(0,zone));
  if(!parsed)error("ValueError","timestamp filter contains invalid native date bound");
  parsed->timezone_aware=true;return parsed;
 }
 if(!text_value(value))error("AttributeError","'"+type_name(value)+"' object has no attribute 'replace'");
 const auto invalid=[&](){error("ValueError","time data "+string_key_repr(value.value)+" does not match format '%Y-%m-%d %H:%M:%S'");};
 static const std::regex pattern(R"(^([0-9]{4})-([0-9]{1,2})-([0-9]{1,2})\s+([0-9]{1,2}):([0-9]{1,2}):([0-9]{1,2}))");
 std::smatch matched;if(!std::regex_search(value.value,matched,pattern))invalid();
 const auto two=[](int n){return (n<10?std::string("0"):std::string{})+std::to_string(n);};
 const int month=std::stoi(matched[2]),day=std::stoi(matched[3]),hour=std::stoi(matched[4]),minute=std::stoi(matched[5]),second=std::stoi(matched[6]);
 if(month<1||month>12||day<1||day>31||hour>23||minute>59||second>61)invalid();
 if(matched.length()!=value.value.size())error("ValueError","unconverted data remains: "+value.value.substr(matched.length()));
 if(second>59)error("ValueError","second must be in 0..59");
 const auto canonical=matched[1].str()+"-"+two(month)+"-"+two(day)+"T"+two(hour)+":"+two(minute)+":"+two(second)+"Z";
 auto parsed=parse_timestamp_exact(canonical);if(!parsed)error("ValueError","day is out of range for month");
 return parsed;
}
std::optional<std::size_t> column(const EventTable& source,const EventAttribute& key){
 if(!text_value(key))return std::nullopt;
 const auto p=std::find(source.columns.begin(),source.columns.end(),key.value);
 return p==source.columns.end()?std::nullopt:std::optional<std::size_t>(p-source.columns.begin());
}
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
const EventAttribute& cell(const EventTable& source,std::size_t row,std::size_t col){
 static const EventAttribute none{"","",AttributeKind::kString,true};
 return source.rows[row][col]?*source.rows[row][col]:none;
}
int compare_instants(const ParsedExactTimestamp& a,const ParsedExactTimestamp& b){
 if(a.seconds!=b.seconds)return a.seconds<b.seconds?-1:1;
 return a.nanoseconds<b.nanoseconds?-1:a.nanoseconds>b.nanoseconds?1:0;
}
bool inside(const std::optional<ParsedExactTimestamp>& value,const std::optional<ParsedExactTimestamp>& lower,const std::optional<ParsedExactTimestamp>& upper){
 return value&&lower&&upper&&compare_instants(*value,*lower)>=0&&compare_instants(*value,*upper)<=0;
}
std::string identity(const EventAttribute& value){
 if(is_native_na_attribute(value))return "missing:NA";
 if(value.is_null)return date_value(value)?"missing:NaT":"missing:None";
 if(missing(value))return "missing:NaN";
 return group_key(value);
}
void check_time(const EventTable& source,std::size_t time,Mode mode,const EventAttribute* sample){
 const bool grouped=mode!=Mode::kEvents&&mode!=Mode::kAttribute;
 const std::string dtype=source.dtypes.empty()?"":source.dtypes[time];
 if(dtype.starts_with("datetime64")&&dtype.find(',')==std::string::npos)
  error("TypeError","Invalid comparison between dtype="+dtype+" and datetime");
 if(!sample||missing(*sample))return;
 if(!date_value(*sample))error("TypeError",std::string(mode==Mode::kIntersecting?"'<'":"'>='")+" not supported between instances of '"+type_name(*sample)+"' and 'datetime.datetime'");
 const auto parsed=instant(*sample);
 if(parsed&&!parsed->timezone_aware){
  if(grouped){const auto* native=std::get_if<NativeDate>(&sample->typed_value);const auto unit=native&&native->resolution==NativeDateResolution::kNanosecond?"ns":"us";error("TypeError",std::string("Invalid comparison between dtype=datetime64[")+unit+"] and datetime");}
  error("TypeError","can't compare offset-naive and offset-aware datetimes");
 }
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
bool equal_value(const EventAttribute& left,const EventAttribute& right){
 if(missing(left)||missing(right))return false;
 if(date_value(left)||date_value(right)){
  if(!date_value(left)||!date_value(right))return false;
  const auto a=instant(left),b=instant(right);
  return a&&b&&a->timezone_aware==b->timezone_aware&&compare_instants(*a,*b)==0;
 }
 if(!root_cause_key_is_hashable(left)||!root_cause_key_is_hashable(right))return false;
 return root_cause_scalar_keys_equal(left,right);
}
struct Group { std::string key;const EventAttribute* minimum=nullptr;const EventAttribute* maximum=nullptr; };
struct Groups { std::vector<Group> values;std::vector<std::optional<std::size_t>> rows; };
Groups aggregate(const EventTable& source,std::size_t cases,std::size_t time,Mode mode){
 Groups result;result.rows.resize(source.size());std::unordered_map<std::string,std::size_t> positions;
 for(std::size_t i=0;i<source.size();++i){
  const auto& id=cell(source,i,cases);if(missing(id))continue;
  const auto key=group_key(id);const auto [p,added]=positions.emplace(key,result.values.size());if(added)result.values.push_back(Group{key});
  result.rows[i]=p->second;auto& group=result.values[p->second];const auto& time_value=cell(source,i,time);if(missing(time_value))continue;
  if(!group.minimum){group.minimum=group.maximum=&time_value;continue;}
  const auto a=native_scalar_order_kind(*group.minimum),b=native_scalar_order_kind(time_value);
  if(a!=b||a==NativeScalarOrderKind::kUnsupported)error("TypeError",std::string("agg function failed [how->")+(mode==Mode::kCompleting?"max":"min")+",dtype->object]");
  // Date extrema must retain adjacent nanoseconds; scalar-key ordering uses
  // microseconds for its established independent grouping contract.
  const auto compare_time=[&](const EventAttribute& left,const EventAttribute& right){
   if(a==NativeScalarOrderKind::kDate)return compare_instants(*instant(left),*instant(right));
   return native_scalar_key_compare(left,right);
  };
  if(compare_time(time_value,*group.minimum)<0)group.minimum=&time_value;
  if(compare_time(time_value,*group.maximum)>0)group.maximum=&time_value;
 }
 return result;
}
EventTable apply_filter(const EventTable& source,const EventAttribute& first,const EventAttribute& last,const PandasTimestampFilterOptions& options,Mode mode,const EventAttribute* attribute=nullptr,const EventAttribute* wanted=nullptr){
 const auto lower=bound(first),upper=bound(last);
 source.validate();const bool keep_missing=truth(options.keep_nan_values);
 if(mode==Mode::kEvents){
  if(options.timestamp_key.container==AttributeContainerKind::kList&&options.timestamp_key.children.empty())return masked_frame(source);
  const auto time=direct_column(source,options.timestamp_key);check_time(source,time,mode,nullptr);
  std::vector<bool> keep(source.size());
  for(std::size_t i=0;i<source.size();++i){const auto& value=cell(source,i,time);check_time(source,time,mode,&value);keep[i]=(missing(value)&&keep_missing)||inside(instant(value),lower,upper);}
  return subset(source,keep);
 }
 if(mode==Mode::kAttribute){
  const bool empty_attribute=attribute->container==AttributeContainerKind::kList&&attribute->children.empty();
  const auto selected=empty_attribute?std::optional<std::size_t>{}:std::optional<std::size_t>{direct_column(source,*attribute)};
  const bool vector=wanted->container==AttributeContainerKind::kList;
  if(vector&&wanted->children.size()!=source.size())error("ValueError","('Lengths must match to compare', ("+std::to_string(source.size())+",), ("+std::to_string(wanted->children.size())+",))");
  if(options.timestamp_key.container==AttributeContainerKind::kList&&options.timestamp_key.children.empty())error("ValueError","Cannot index with multidimensional key");
  const auto time=direct_column(source,options.timestamp_key);check_time(source,time,mode,nullptr);
  // A datetime64 attribute parses a string value like pandas' comparison
  // (round BY): the instant, the column zone for a naive text, NaT texts
  // matching nothing.
  const std::string attribute_dtype=selected&&!source.dtypes.empty()?source.dtypes[*selected]:"object";
  const bool datetime_attribute=attribute_dtype.starts_with("datetime64");
  const auto attribute_zone=pandas_datetime_target::zone(attribute_dtype);
  const auto matches=[&](const EventAttribute& value,const EventAttribute& target){
   if(!datetime_attribute||!text_value(target))return equal_value(value,target);
   if(missing(value)||!date_value(value))return false;
   const auto a=instant(value);const auto b=pandas_datetime_target::instant(target.value,attribute_zone,"pandas datetime column string values must be ISO-8601 timestamps");
   return a&&b&&a->timezone_aware==b->timezone_aware&&compare_instants(*a,*b)==0;
  };
  std::vector<std::size_t> candidates;
  for(std::size_t i=0;i<source.size();++i){if(!selected||!matches(cell(source,i,*selected),vector?wanted->children[i]:*wanted))continue;
   // keep_nan_values also accepts the cases of matching rows without a time.
   const auto& value=cell(source,i,time);check_time(source,time,mode,&value);if(inside(instant(value),lower,upper)||(keep_missing&&missing(value)))candidates.push_back(i);
  }
  if(options.case_id_key.container==AttributeContainerKind::kList&&options.case_id_key.children.empty())error("AttributeError","'DataFrame' object has no attribute 'unique'");
  const auto cases=direct_column(source,options.case_id_key);std::unordered_set<std::string> accepted;
  for(auto i:candidates)accepted.insert(identity(cell(source,i,cases)));
  // Series.isin on a masked numeric column (Int64, Float64, ...) never
  // matches a <NA> cell, even when the accepted cases hold one (round BZ).
  const bool masked=cases<source.dtypes.size()&&detail::masked_numeric_dtype(source.dtypes[cases]);
  std::vector<bool> keep(source.size());
  for(std::size_t i=0;i<source.size();++i){const auto& id=cell(source,i,cases);keep[i]=!(masked&&(id.is_null||is_native_na_attribute(id)))&&accepted.contains(identity(id));}
  return subset(source,keep);
 }
 if(options.case_id_key.is_null)error("TypeError","You have to supply one of 'by' and 'level'");
 if(options.case_id_key.container==AttributeContainerKind::kList&&options.case_id_key.children.empty())error("ValueError","No group keys passed!");
 const auto cases=direct_column(source,options.case_id_key);
 if(options.timestamp_key.container==AttributeContainerKind::kList&&options.timestamp_key.children.empty()){
  if(mode==Mode::kContained||mode==Mode::kIntersecting)error("ValueError","No objects to concatenate");
  error("AttributeError","'DataFrame' object has no attribute 'to_frame'");
 }
 const auto time=column(source,options.timestamp_key);if(!time)error("KeyError",string_key_repr("Column not found: "+(text_value(options.timestamp_key)?options.timestamp_key.value:repr(options.timestamp_key))));
 const auto grouped=aggregate(source,cases,*time,mode);check_time(source,*time,mode,nullptr);
 std::vector<bool> matched(grouped.values.size());
 for(std::size_t i=0;i<grouped.values.size();++i){const auto& group=grouped.values[i];
  if(!group.minimum){matched[i]=keep_missing;continue;}
  check_time(source,*time,mode,mode==Mode::kCompleting?group.maximum:group.minimum);
  const auto a=instant(*group.minimum),b=instant(*group.maximum);
  if(mode==Mode::kStarting)matched[i]=inside(a,lower,upper);
  else if(mode==Mode::kCompleting)matched[i]=inside(b,lower,upper);
  else if(mode==Mode::kContained)matched[i]=a&&b&&lower&&upper&&compare_instants(*a,*lower)>=0&&compare_instants(*b,*upper)<=0;
  else matched[i]=a&&b&&lower&&upper&&compare_instants(*a,*upper)<0&&compare_instants(*b,*lower)>0;
 }
 const bool positive=mode==Mode::kStarting||mode==Mode::kCompleting?truth(options.positive):true;
 std::vector<bool> keep(source.size());for(std::size_t i=0;i<source.size();++i){const bool match=grouped.rows[i]&&matched[*grouped.rows[i]];keep[i]=match==positive;}
 return subset(source,keep);
}
PandasTimestampFilterOptions resolve_timestamp_options(const std::vector<PandasTimestampFilterParameter>& parameters){
 PandasTimestampFilterOptions options;
 const auto assign=[&](const EventAttribute& p){if(p.key=="pm4py:param:case_id_key")options.case_id_key=p;else if(p.key=="pm4py:param:timestamp_key")options.timestamp_key=p;else if(p.key=="positive")options.positive=p;else if(p.key=="keep_nan_values")options.keep_nan_values=p;};
 for(const auto& p:parameters)if(!p.enum_key)assign(p.value);
 for(const auto& p:parameters)if(p.enum_key)assign(p.value);
 return options;
}
PandasTimestampFilterOptions resolve_timestamp_options(const EventAttribute& parameters){
 if(parameters.is_null||(parameters.container==AttributeContainerKind::kList&&parameters.children.empty()))return {};
 if(parameters.container==AttributeContainerKind::kContainer){std::vector<PandasTimestampFilterParameter> values;for(const auto& p:parameters.children)values.push_back({p,false});return resolve_timestamp_options(values);}
 detail::check_parameter_carrier(parameters,[](const std::string& type,const std::string& message){error(type,message);});
 return {};
}
} // namespace
#define MINERA_TIMESTAMP_FUNCTION(name,mode) \
EventTable name(const EventTable& source,const EventAttribute& a,const EventAttribute& b,const PandasTimestampFilterOptions& o){return apply_filter(source,a,b,o,Mode::mode);} \
EventTable name(const EventTable& source,const EventAttribute& a,const EventAttribute& b,const EventAttribute& p){return name(source,a,b,resolve_timestamp_options(p));} \
EventTable name(const EventTable& source,const EventAttribute& a,const EventAttribute& b,const std::vector<PandasTimestampFilterParameter>& p){return name(source,a,b,resolve_timestamp_options(p));}
MINERA_TIMESTAMP_FUNCTION(filter_pandas_timestamp_events,kEvents)
MINERA_TIMESTAMP_FUNCTION(filter_pandas_timestamp_contained,kContained)
MINERA_TIMESTAMP_FUNCTION(filter_pandas_timestamp_intersecting,kIntersecting)
MINERA_TIMESTAMP_FUNCTION(filter_pandas_timestamp_starting,kStarting)
MINERA_TIMESTAMP_FUNCTION(filter_pandas_timestamp_completing,kCompleting)
#undef MINERA_TIMESTAMP_FUNCTION
EventTable filter_pandas_timestamp_attribute(const EventTable& s,const EventAttribute& key,const EventAttribute& value,const EventAttribute& a,const EventAttribute& b,const PandasTimestampFilterOptions& o){return apply_filter(s,a,b,o,Mode::kAttribute,&key,&value);}
EventTable filter_pandas_timestamp_attribute(const EventTable& s,const EventAttribute& key,const EventAttribute& value,const EventAttribute& a,const EventAttribute& b,const EventAttribute& p){return filter_pandas_timestamp_attribute(s,key,value,a,b,resolve_timestamp_options(p));}
EventTable filter_pandas_timestamp_attribute(const EventTable& s,const EventAttribute& key,const EventAttribute& value,const EventAttribute& a,const EventAttribute& b,const std::vector<PandasTimestampFilterParameter>& p){return filter_pandas_timestamp_attribute(s,key,value,a,b,resolve_timestamp_options(p));}
EventTable apply_pandas_timestamp_filter(const EventTable&,const EventAttribute&){error("Exception","apply method not available for timestamp filter");}
EventTable auto_filter_pandas_timestamp(const EventTable&,const EventAttribute&){error("Exception","apply_auto_filter method not available for timestamp filter");}
} // namespace minera
