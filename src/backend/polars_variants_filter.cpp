#include "minera/polars_variants_filter.hpp"

#include <algorithm>
#include <charconv>
#include <bit>
#include <limits>
#include <cmath>
#include <numeric>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {

using Cell = std::optional<EventAttribute>;
using EnumRanks = std::unordered_map<std::string, std::size_t>;
enum class Dtype { kString, kEnum, kInteger, kFloat, kBoolean, kNull, kDate };

bool categorical_dtype(const std::string& value) {
  return value == "Categorical" || (value.starts_with("Categorical(") && value.ends_with(')'));
}
bool enum_dtype(const std::string& value) {
  return value.starts_with("Enum(") && value.ends_with(')');
}

[[noreturn]] void fail(std::string type, std::string phase, std::string message) {
  throw PolarsVariantsFilterError(std::move(type), std::move(phase), std::move(message));
}

bool null(const EventAttribute& value) { return value.is_null || is_native_na_attribute(value); }
bool collection(const EventAttribute& value) { return value.container != AttributeContainerKind::kScalar; }
bool boolean(const EventAttribute& value) {
  return value.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value);
}
bool integer(const EventAttribute& value) {
  return value.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) ||
         std::holds_alternative<std::uint64_t>(value.typed_value);
}
bool real(const EventAttribute& value) {
  return value.kind == AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value);
}
bool date(const EventAttribute& value) {
  return value.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value);
}
bool text(const EventAttribute& value) {
  return !collection(value) && !null(value) && !boolean(value) && !integer(value) && !real(value) && !date(value);
}
TokenInteger exact(const EventAttribute& value) {
  if (const auto* v = std::get_if<std::int64_t>(&value.typed_value)) return TokenInteger(*v);
  if (const auto* v = std::get_if<std::uint64_t>(&value.typed_value)) return TokenInteger(*v);
  if (const auto* v = std::get_if<bool>(&value.typed_value)) return TokenInteger(*v ? 1 : 0);
  if (boolean(value)) return TokenInteger(value.value == "true" || value.value == "1" ? 1 : 0);
  return TokenInteger(value.value);
}
double number(const EventAttribute& value) {
  return integer(value) || boolean(value) ? static_cast<double>(exact(value)) : native_attribute_number(value);
}
std::string real_text(double value, bool cast_string = false) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  char buffer[128];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (result.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars real value");
  std::string output(buffer, result.ptr);
  if (cast_string && output.find_first_of(".eE") == std::string::npos) output += ".0";
  return output;
}
std::string quote(const std::string& value) { return '"' + value + '"'; }
std::string debug_quote(const std::string& value) {
  std::string output = "\"";
  for (const auto c : value) {
    switch (c) {
      case '\\': output += "\\\\"; break;
      case '"': output += "\\\""; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      case '\0': output += "\\0"; break;
      default: output += c;
    }
  }
  return output + '"';
}
std::string python_type(const EventAttribute& value) {
  if (collection(value)) {
    switch (value.container) {
      case AttributeContainerKind::kList: return "list";
      case AttributeContainerKind::kTuple: return "tuple";
      case AttributeContainerKind::kSet: return "set";
      case AttributeContainerKind::kFrozenSet: return "frozenset";
      default: return "dict";
    }
  }
  if (null(value)) return "NoneType";
  if (boolean(value)) return "bool";
  if (integer(value)) return "int";
  if (real(value)) return "float";
  if (date(value)) return "datetime";
  return "str";
}


Dtype dtype(const std::string& value) {
  if (value == "String" || categorical_dtype(value)) return Dtype::kString;
  if (enum_dtype(value)) return Dtype::kEnum;
  if (value == "Int64" || value == "UInt64" || value == "UInt32") return Dtype::kInteger;
  if (value == "Float64") return Dtype::kFloat;
  if (value == "Boolean") return Dtype::kBoolean;
  if (value == "Null") return Dtype::kNull;
  if (value == "Date" || value.starts_with("Datetime(time_unit='us', time_zone=")) return Dtype::kDate;
  throw std::invalid_argument("Unsupported native Polars variants dtype: " + value);
}
std::string frame_plan(const EventTable& source) {
  std::string output = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, source.columns.size()); ++i) {
    if (i) output += ", ";
    output += quote(source.columns[i]);
  }
  if (source.columns.size() > 4) output += ", ...";
  return output + "]; PROJECT */" + std::to_string(source.columns.size()) + " COLUMNS";
}

int compare(const Cell& left, const Cell& right, Dtype kind,
            const EnumRanks* dictionary = nullptr) {
  const auto absent = [kind](const Cell& value) { return !value || null(*value) || kind == Dtype::kNull; };
  if (absent(left) || absent(right)) return absent(left) == absent(right) ? 0 : absent(left) ? -1 : 1;
  if (kind == Dtype::kString) return left->value < right->value ? -1 : right->value < left->value ? 1 : 0;
  if (kind == Dtype::kEnum) {
    if (!dictionary) throw std::invalid_argument("Native Polars variants Enum ordering requires its category dictionary");
    const auto position = [&](const std::string& value) {
      const auto found = dictionary->find(value);
      if (found == dictionary->end())
        throw std::invalid_argument("Native Polars variants Enum cell is absent from its category dictionary");
      return found->second;
    };
    const auto a = position(left->value), b = position(right->value);
    return a < b ? -1 : a > b ? 1 : 0;
  }
  if (kind == Dtype::kInteger || kind == Dtype::kBoolean) {
    const auto a = exact(*left), b = exact(*right); return a < b ? -1 : a > b ? 1 : 0;
  }
  if (kind == Dtype::kFloat) {
    const auto a = number(*left), b = number(*right);
    if (std::isnan(a) || std::isnan(b)) return std::isnan(a) == std::isnan(b) ? 0 : std::isnan(a) ? 1 : -1;
    return a < b ? -1 : a > b ? 1 : 0;
  }
  const auto instant = [](const EventAttribute& value) {
    const auto* stored = std::get_if<NativeDate>(&value.typed_value);
    const auto parsed = parse_timestamp_exact(stored ? stored->value : value.value);
    if (!parsed) throw std::invalid_argument("Unsupported native Polars variants temporal cell");
    return *parsed;
  };
  const auto a = instant(*left), b = instant(*right);
  if (a.seconds != b.seconds) return a.seconds < b.seconds ? -1 : 1;
  return a.nanoseconds < b.nanoseconds ? -1 : a.nanoseconds > b.nanoseconds ? 1 : 0;
}

std::string string_cast(const EventAttribute& value, const std::string& declared = "") {
  if (declared == "String" || categorical_dtype(declared) || enum_dtype(declared)) return value.value;
  if (text(value)) return value.value;
  if (boolean(value)) return static_cast<bool>(exact(value)) ? "true" : "false";
  if (integer(value)) return exact(value).decimal();
  if (real(value)) return real_text(number(value), true);
  if (date(value)) {
    const auto lexical=std::get_if<NativeDate>(&value.typed_value)?std::get<NativeDate>(value.typed_value).value:value.value;
    if(declared=="Date")return lexical.substr(0,10);
    const auto parsed=parse_timestamp_exact(lexical);if(!parsed)throw std::invalid_argument("Native Polars variants temporal cast requires valid declared datetime");
    auto result=format_timestamp(static_cast<double>(parsed->seconds)).substr(0,19);std::replace(result.begin(),result.end(),'T',' ');
    result+='.'+std::to_string(1000000+parsed->nanoseconds/1000).substr(1);
    if(declared.find("time_zone='UTC'")!=std::string::npos)result+="+00:00";return result;
  }
  if (value.container == AttributeContainerKind::kContainer) {
    std::string output = "{";
    for (std::size_t i = 0; i < value.children.size(); ++i) {
      if (i) output += ',';
      output += string_cast(value.children[i]);
    }
    return output + '}';
  }
  throw std::invalid_argument("Unsupported native Polars variants string cast");
}

bool truth(const EventAttribute& value) {
  if (null(value)) return false;
  if (collection(value)) return !value.children.empty();
  if (integer(value) || boolean(value)) return static_cast<bool>(exact(value));
  if (real(value)) return number(value) != 0;
  return !value.value.empty();
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
std::vector<EventAttribute> iter(const EventAttribute& value) {
  if(value.container==AttributeContainerKind::kContainer){std::vector<EventAttribute> keys;for(const auto& child:value.children)keys.push_back({"",child.key});return keys;}
  if(collection(value))return value.children;
  if(text(value)){std::vector<EventAttribute> chars;for(std::size_t i=0;i<value.value.size();){const auto c=static_cast<unsigned char>(value.value[i]);const auto n=c<0x80?1:c<0xe0?2:c<0xf0?3:4;chars.push_back({"",value.value.substr(i,n)});i+=n;}return chars;}
  fail("TypeError","apply","'"+python_type(value)+"' object is not iterable");
}
std::string join_type(const EventAttribute& value) {
  if(date(value)){const auto lexical=std::get_if<NativeDate>(&value.typed_value)?std::get<NativeDate>(value.typed_value).value:value.value;return lexical.size()==10?"datetime.date":"datetime.datetime";}
  return python_type(value);
}
std::string join(const EventAttribute& sequence) {
  if(!collection(sequence)&&!text(sequence))fail("TypeError","apply","can only join an iterable");
  const auto items=iter(sequence);std::string result;
  for(std::size_t i=0;i<items.size();++i){if(!text(items[i]))fail("TypeError","apply","sequence item "+std::to_string(i)+": expected str instance, "+join_type(items[i])+" found");if(i)result+=',';result+=items[i].value;}
  return result;
}
std::vector<std::string> admitted(const EventAttribute& values){std::vector<std::string> result;for(const auto& sequence:iter(values))result.push_back(join(sequence));return result;}
std::string contexts(const std::vector<std::string>& nodes){std::string out="\n\nThis error occurred with the following context stack:\n";for(std::size_t i=0;i<nodes.size();++i)out+="\t["+std::to_string(i+1)+"] '"+nodes[i]+"'\n";return out;}
std::string sort_plan(const EventTable& source,const PolarsVariantsFilterOptions& options,bool statistics){return "SORT BY [col("+quote(options.case_id_key.value)+"), col("+quote(statistics?options.timestamp_key.value:"time:timestamp")+")]\n  "+frame_plan(source);}
std::string missing_prefix(const std::string& key,const std::vector<std::string>& columns){std::string out="unable to find column "+debug_quote(key)+"; valid columns: [";for(std::size_t i=0;i<columns.size();++i){if(i)out+=", ";out+=debug_quote(columns[i]);}out+=']';for(const auto& candidate:columns)if(!candidate.empty()&&candidate.size()<key.size()&&key.ends_with(candidate)&&5*candidate.size()>=2*(key.size()+candidate.size())){out+="\n\nDid you mean "+debug_quote(candidate)+'?';break;}return out;}
[[noreturn]] void missing(const EventTable& source,const std::string& key,bool activity,bool statistics,const PolarsVariantsFilterOptions& options,const std::string& phase,bool eager_source){
  if(eager_source){
    const auto prefix=missing_prefix(key,source.columns);
    if(!activity)fail("ColumnNotFoundError",phase,prefix);
    fail("ColumnNotFoundError",phase,prefix+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n"+frame_plan(source)+contexts({"group_by"}));
  }
  std::vector<std::string> nodes;if(!activity)nodes.push_back("sort");nodes.push_back("group_by");
  if(!statistics){nodes.insert(nodes.end(),{"filter","select","join right","join"});}
  fail("ColumnNotFoundError",phase,missing_prefix(key,source.columns)+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '"+(statistics?"group_by":"join")+"' <---\n"+(activity?sort_plan(source,options,statistics):frame_plan(source))+contexts(nodes));
}
void validate(const PolarsVariantsFilterOptions& options){
  if(!text(options.activity_key)&&!collection(options.activity_key))fail("TypeError","apply","invalid input for `col`\n\nExpected `str` or `DataType`, got '"+python_type(options.activity_key)+"'.");
  if(collection(options.activity_key)&&!options.activity_key.children.empty())throw std::invalid_argument("Native Polars variants activity selector outside observed empty-list domain");
  if(null(options.case_id_key))fail("ValueError","apply","must specify `on` OR `left_on` and `right_on`");
}
struct Group{std::string key,label;bool absent=false;std::vector<Cell> activities;};
struct Grouped{std::vector<Group> groups;std::size_t case_at=0;std::string case_dtype,activity_dtype;};
Grouped grouped(const EventTable& source,const PolarsVariantsFilterOptions& options,bool statistics,const std::string& phase,bool eager_source=false){
  source.validate();if(source.index||source.dtypes.size()!=source.columns.size())throw std::invalid_argument("Native Polars variants require declared dtypes and no dataframe index");
  if(collection(options.case_id_key)&&!statistics){
    if(!options.case_id_key.children.empty())throw std::invalid_argument("Native Polars variants case selector outside observed empty-list domain");
    fail("InvalidOperationError",phase,"expected join keys/predicates\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nDF []; PROJECT */0 COLUMNS"+contexts({"join"}));
  }
  if(collection(options.case_id_key)||integer(options.case_id_key))fail("ShapeError",phase,"sort expressions must have same length as DataFrame, got DataFrame height: "+std::to_string(source.rows.size())+" and Series length: 1");
  const auto position=[&](const std::string& key,bool activity=false){auto found=std::find(source.columns.begin(),source.columns.end(),key);if(found==source.columns.end())missing(source,key,activity,statistics,options,phase,eager_source||(!statistics&&phase=="apply"));return static_cast<std::size_t>(found-source.columns.begin());};
  const auto case_at=position(options.case_id_key.value),time_at=position(statistics?options.timestamp_key.value:"time:timestamp");
  if(collection(options.activity_key)){
    if(statistics)return {};
    fail("ColumnNotFoundError",phase,missing_prefix("variant",{options.case_id_key.value})+"\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nAGGREGATE[maintain_order: true]\n  [] BY [col("+quote(options.case_id_key.value)+")]\n  FROM\n  SORT BY [col("+quote(options.case_id_key.value)+"), col(\"time:timestamp\")]\n    "+frame_plan(source)+contexts({"select","join right","join"}));
  }
  const auto activity_at=position(options.activity_key.value,true);const auto case_kind=dtype(source.dtypes[case_at]),time_kind=dtype(source.dtypes[time_at]);
  const auto dictionary=[&](std::size_t at,Dtype kind)->std::optional<EnumRanks>{
    if(kind!=Dtype::kEnum)return std::nullopt;
    if(source.categorical_dtypes.empty()||!source.categorical_dtypes[at])throw std::invalid_argument("Native Polars variants require an Enum dictionary for selected sort column: "+source.columns[at]);
    EnumRanks ranks;const auto& labels=source.categorical_dtypes[at]->categories;ranks.reserve(labels.size());for(std::size_t i=0;i<labels.size();++i)ranks.emplace(labels[i].value,i);
    for(const auto& row:source.rows)if(row[at]&&!null(*row[at])&&!ranks.contains(row[at]->value))throw std::invalid_argument("Native Polars variants Enum cell absent from selected dictionary: "+source.columns[at]);return ranks;
  };
  const auto case_ranks=dictionary(case_at,case_kind),time_ranks=dictionary(time_at,time_kind);
  std::vector<std::size_t> order(source.rows.size());std::iota(order.begin(),order.end(),0);
  std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){const auto bycase=compare(source.rows[a][case_at],source.rows[b][case_at],case_kind,case_ranks?&*case_ranks:nullptr);return bycase?bycase<0:compare(source.rows[a][time_at],source.rows[b][time_at],time_kind,time_ranks?&*time_ranks:nullptr)<0;});
  Grouped result;result.case_at=case_at;result.case_dtype=source.dtypes[case_at];result.activity_dtype=source.dtypes[activity_at];
  for(std::size_t at=0;at<order.size();){const auto key=group_key(source.rows[order[at]][case_at],source.dtypes[case_at]);auto end=at+1;while(end<order.size()&&group_key(source.rows[order[end]][case_at],source.dtypes[case_at])==key)++end;
    Group group;group.key=key;group.absent=key=="n";bool first=true;
    for(auto i=at;i<end;++i){const auto& cell=source.rows[order[i]][activity_at];group.activities.push_back(cell);if(!cell||null(*cell)||source.dtypes[activity_at]=="Null")continue;if(!first)group.label+=',';group.label+=string_cast(*cell,source.dtypes[activity_at]);first=false;}
    result.groups.push_back(std::move(group));at=end;
  }
  return result;
}
EventTable collect(const EventTable& source,const std::vector<std::string>& labels,const PolarsVariantsFilterOptions& options,const std::string& phase){
  const auto groups=grouped(source,options,false,phase);std::unordered_map<std::string,bool> matches;
  for(const auto& group:groups.groups)matches.emplace(group.key,!group.absent&&std::find(labels.begin(),labels.end(),group.label)!=labels.end());
  auto result=source;result.rows.clear();for(const auto& row:source.rows){const auto key=group_key(row[groups.case_at],groups.case_dtype);if(matches.at(key)==truth(options.positive))result.rows.push_back(row);}return result;
}
LazyEventTable lazy(const LazyEventTable& source,std::vector<std::string> labels,const PolarsVariantsFilterOptions& options){
  validate(options);return LazyEventTable(source.columns(),[source,labels=std::move(labels),options](const auto& consume){const auto result=collect(source.materialize(),labels,options,"collect");for(const auto& row:result.rows)consume(row);},source.attributes(),source.metadata(),std::nullopt,source.dtypes(),source.index(),source.categorical_dtypes());
}
bool py_equal(const Cell& a,const Cell& b){if(!a||null(*a))return !b||null(*b);if(!b||null(*b))return false;if((integer(*a)||boolean(*a))&&(integer(*b)||boolean(*b)))return exact(*a)==exact(*b);if((real(*a)||integer(*a)||boolean(*a))&&(real(*b)||integer(*b)||boolean(*b)))return number(*a)==number(*b);if(date(*a)&&date(*b))return compare(a,b,Dtype::kDate)==0;return text(*a)&&text(*b)?a->value==b->value:a==b;}
bool py_less(const Cell& a,const Cell& b){
  if(!a||null(*a)||!b||null(*b))fail("TypeError","apply","'<' not supported between instances of '"+std::string(!a||null(*a)?"NoneType":python_type(*a))+"' and '"+std::string(!b||null(*b)?"NoneType":python_type(*b))+"'");
  if((integer(*a)||boolean(*a))&&(integer(*b)||boolean(*b)))return exact(*a)<exact(*b);
  if((real(*a)||integer(*a)||boolean(*a))&&(real(*b)||integer(*b)||boolean(*b)))return number(*a)<number(*b);
  if(text(*a)&&text(*b))return a->value<b->value;
  if(date(*a)&&date(*b))return compare(a,b,Dtype::kDate)<0;
  fail("TypeError","apply","'<' not supported between instances of '"+python_type(*a)+"' and '"+python_type(*b)+"'");
}
bool tuple_less(const std::vector<Cell>& a,const std::vector<Cell>& b){for(std::size_t i=0;i<std::min(a.size(),b.size());++i){if(py_equal(a[i],b[i]))continue;return py_less(a[i],b[i]);}return a.size()<b.size();}
struct Variant{std::vector<Cell> activities;std::size_t count=0;};
// CPython's short-list reverse sort observes this comparison order before
// joining tuple values; it matters for the retained mixed None/scalar errors.
void sort_variants(std::vector<Variant>& values){
  const auto less_variant=[](const Variant& a,const Variant& b){return a.count!=b.count?a.count<b.count:tuple_less(a.activities,b.activities);};
  if(values.size()>=64){std::stable_sort(values.begin(),values.end(),[&](const auto& a,const auto& b){return less_variant(b,a);});return;}
  std::reverse(values.begin(),values.end());
  if(values.size()>1){std::size_t run=2;const bool descending=less_variant(values[1],values[0]);
    if(descending){while(run<values.size()&&less_variant(values[run],values[run-1]))++run;std::reverse(values.begin(),values.begin()+static_cast<std::ptrdiff_t>(run));}
    else while(run<values.size()&&!less_variant(values[run],values[run-1]))++run;
    for(auto i=run;i<values.size();++i){auto pivot=std::move(values[i]);std::size_t left=0,right=i;while(left<right){const auto mid=left+(right-left)/2;if(less_variant(pivot,values[mid]))right=mid;else left=mid+1;}for(auto j=i;j>left;--j)values[j]=std::move(values[j-1]);values[left]=std::move(pivot);}
  }
  std::reverse(values.begin(),values.end());
}
std::vector<std::string> top_labels(const EventTable& source,const EventAttribute& maximum,const PolarsVariantsFilterOptions& options,bool eager){
  const auto groups=grouped(source,options,true,"apply",eager);if(eager)fail("AttributeError","apply","'DataFrame' object has no attribute 'collect'");
  if(collection(options.activity_key))fail("IndexError","apply","tuple index out of range");
  std::vector<Variant> variants;std::unordered_map<std::string,std::size_t> positions;
  for(const auto& group:groups.groups){
    std::string key;for(const auto& cell:group.activities){const auto item=group_key(cell,groups.activity_dtype);key+=std::to_string(item.size())+':'+item;}
    const auto [found,inserted]=positions.emplace(std::move(key),variants.size());if(inserted)variants.push_back({group.activities,1});else ++variants[found->second].count;
  }
  sort_variants(variants);
  const auto count=variants.size();std::size_t take=count;
  if(integer(maximum)||boolean(maximum)){const auto k=exact(maximum);if(k<TokenInteger(count)){const auto length=k<TokenInteger(0)?TokenInteger(count)+k:k;take=length<TokenInteger(0)?0:static_cast<std::size_t>(length);}}
  else if(real(maximum)){if(!(static_cast<double>(count)<number(maximum)))fail("TypeError","apply","slice indices must be integers or None or have an __index__ method");}
  else fail("TypeError","apply","'<' not supported between instances of 'int' and '"+python_type(maximum)+"'");
  std::vector<std::string> result;
  for(std::size_t i=0;i<take;++i){EventAttribute sequence;sequence.container=AttributeContainerKind::kTuple;for(const auto& cell:variants[i].activities){EventAttribute value;if(cell)value=*cell;else value.is_null=true;sequence.children.push_back(std::move(value));}result.push_back(join(sequence));}return result;
}
std::vector<PolarsVariantsFilterParameter> parameters(const EventAttribute& value){
  if(null(value))return {};
  if(value.container==AttributeContainerKind::kContainer){std::vector<PolarsVariantsFilterParameter> result;for(const auto& child:value.children)result.push_back({child.key,child,false});return result;}
  if(value.container==AttributeContainerKind::kList){if(value.children.empty())return {};fail("TypeError","apply","list indices must be integers or slices, not str");}
  if(text(value))fail("TypeError","apply","string indices must be integers, not 'str'");
  fail("TypeError","apply","'"+python_type(value)+"' object is not iterable");
}
PolarsVariantsFilterOptions statistics_parameters(const std::vector<PolarsVariantsFilterParameter>& values) {
  // The public statistics companion unrolls keys in insertion order. Final
  // membership separately retains its own enum-preferred parameter lookup.
  PolarsVariantsFilterOptions options;
  for(const auto& value:values) {
    if(value.key=="pm4py:param:case_id_key")options.case_id_key=value.value;
    else if(value.key=="pm4py:param:activity_key")options.activity_key=value.value;
    else if(value.key=="pm4py:param:timestamp_key")options.timestamp_key=value.value;
    else if(value.key=="positive")options.positive=value.value;
  }
  return options;
}
[[noreturn]] void unavailable(){fail("AttributeError","apply","module 'pm4py.util.constants' has no attribute 'DEFAULT_NAME_KEY'");}
[[noreturn]] void missing_argument(const std::string& function,const std::string& argument){fail("TypeError","apply",function+"() missing 1 required positional argument: '"+argument+"'");}
}  // namespace

PolarsVariantsFilterOptions resolve_polars_variants_filter_parameters(const std::vector<PolarsVariantsFilterParameter>& values){PolarsVariantsFilterOptions options;for(bool enum_key:{false,true})for(const auto& value:values){if(value.enum_key!=enum_key)continue;if(value.key=="pm4py:param:case_id_key")options.case_id_key=value.value;else if(value.key=="pm4py:param:activity_key")options.activity_key=value.value;else if(value.key=="pm4py:param:timestamp_key")options.timestamp_key=value.value;else if(value.key=="positive")options.positive=value.value;}return options;}
EventTable filter_polars_variants(const EventTable& source,const EventAttribute& values,const PolarsVariantsFilterOptions& options){validate(options);return collect(source,admitted(values),options,"apply");}
LazyEventTable filter_polars_variants(const LazyEventTable& source,const EventAttribute& values,const PolarsVariantsFilterOptions& options){validate(options);return lazy(source,admitted(values),options);}
EventTable filter_polars_variants_top_k(const EventTable& source,const EventAttribute& maximum,const PolarsVariantsFilterOptions& options){validate(options);(void)top_labels(source,maximum,options,true);throw std::logic_error("Unreachable eager Polars top-k");}
LazyEventTable filter_polars_variants_top_k(const LazyEventTable& source,const EventAttribute& maximum,const PolarsVariantsFilterOptions& options){validate(options);return lazy(source,top_labels(source.materialize(),maximum,options,false),options);}
EventTable filter_polars_variants_from_list(const EventTable&,const EventAttribute&,const PolarsVariantsFilterOptions&){unavailable();}
LazyEventTable filter_polars_variants_from_list(const LazyEventTable&,const EventAttribute&,const PolarsVariantsFilterOptions&){unavailable();}
EventTable auto_filter_polars_variants(const EventTable&,const PolarsVariantsFilterOptions&){unavailable();}
LazyEventTable auto_filter_polars_variants(const LazyEventTable&,const PolarsVariantsFilterOptions&){unavailable();}
EventTable filter_polars_variants_parameters(const EventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants(s,v,resolve_polars_variants_filter_parameters(p));}
EventTable filter_polars_variants_parameter_value(const EventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_parameters(s,v,parameters(p));}
EventTable filter_polars_variants(const EventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply","admitted_variants");}
LazyEventTable filter_polars_variants_parameters(const LazyEventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants(s,v,resolve_polars_variants_filter_parameters(p));}
LazyEventTable filter_polars_variants_parameter_value(const LazyEventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_parameters(s,v,parameters(p));}
LazyEventTable filter_polars_variants(const LazyEventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply","admitted_variants");}
EventTable filter_polars_variants_from_list_parameters(const EventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants_from_list(s,v,resolve_polars_variants_filter_parameters(p));}
EventTable filter_polars_variants_from_list_parameter_value(const EventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_from_list_parameters(s,v,parameters(p));}
EventTable filter_polars_variants_from_list(const EventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply_from_variant_list","variants");}
LazyEventTable filter_polars_variants_from_list_parameters(const LazyEventTable& s,const EventAttribute& v,const std::vector<PolarsVariantsFilterParameter>& p){return filter_polars_variants_from_list(s,v,resolve_polars_variants_filter_parameters(p));}
LazyEventTable filter_polars_variants_from_list_parameter_value(const LazyEventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_from_list_parameters(s,v,parameters(p));}
LazyEventTable filter_polars_variants_from_list(const LazyEventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("apply_from_variant_list","variants");}
EventTable filter_polars_variants_top_k_parameters(const EventTable& source,const EventAttribute& maximum,const std::vector<PolarsVariantsFilterParameter>& values) {
  const auto statistics=statistics_parameters(values);validate(statistics);
  (void)top_labels(source,maximum,statistics,true);throw std::logic_error("Unreachable eager Polars top-k");
}
EventTable filter_polars_variants_top_k_parameter_value(const EventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_top_k_parameters(s,v,parameters(p));}
EventTable filter_polars_variants_top_k(const EventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("filter_variants_top_k","k");}
LazyEventTable filter_polars_variants_top_k_parameters(const LazyEventTable& source,const EventAttribute& maximum,const std::vector<PolarsVariantsFilterParameter>& values) {
  const auto statistics=statistics_parameters(values);validate(statistics);
  const auto labels=top_labels(source.materialize(),maximum,statistics,false);
  return lazy(source,labels,resolve_polars_variants_filter_parameters(values));
}
LazyEventTable filter_polars_variants_top_k_parameter_value(const LazyEventTable& s,const EventAttribute& v,const EventAttribute& p){return filter_polars_variants_top_k_parameters(s,v,parameters(p));}
LazyEventTable filter_polars_variants_top_k(const LazyEventTable&,std::nullopt_t,const PolarsVariantsFilterOptions&){missing_argument("filter_variants_top_k","k");}
EventTable auto_filter_polars_variants_parameters(const EventTable& s,const std::vector<PolarsVariantsFilterParameter>& p){return auto_filter_polars_variants(s,resolve_polars_variants_filter_parameters(p));}
EventTable auto_filter_polars_variants_parameter_value(const EventTable& s,const EventAttribute& p){return auto_filter_polars_variants_parameters(s,parameters(p));}
LazyEventTable auto_filter_polars_variants_parameters(const LazyEventTable& s,const std::vector<PolarsVariantsFilterParameter>& p){return auto_filter_polars_variants(s,resolve_polars_variants_filter_parameters(p));}
LazyEventTable auto_filter_polars_variants_parameter_value(const LazyEventTable& s,const EventAttribute& p){return auto_filter_polars_variants_parameters(s,parameters(p));}
}  // namespace minera
