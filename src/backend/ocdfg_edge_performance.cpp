#include "minera/ocdfg_edge_performance.hpp"
#include "business_hours.hpp"
#include "ocel_frame_types.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace minera { namespace {
[[noreturn]] void missing(const std::string& key,bool column=false){throw OcdfgPerformanceError("KeyError","'"+(column?"Column not found: ":std::string())+key+"'");}
std::vector<BusinessHourSlot> slots(const OcdfgBusinessHourSlots& value){
 if(std::holds_alternative<std::nullptr_t>(value))throw OcdfgPerformanceError("TypeError","'NoneType' object is not iterable");
 if(const auto* text=std::get_if<std::vector<std::string>>(&value)){
  if(!text->empty())throw OcdfgPerformanceError("ValueError",text->front().size()>2?"too many values to unpack (expected 2)":"not enough values to unpack (expected 2, got "+std::to_string(text->front().size())+")");
  return {};
 }
 std::vector<BusinessHourSlot> result;
 if(const auto* configured=std::get_if<std::vector<BusinessHourSlot>>(&value))result=*configured;
 else for(unsigned day=0;day<5;++day)result.push_back({day*86400.0+25200,day*86400.0+61200});
 for(const auto& slot:result)if(!std::isfinite(slot.start_seconds)||!std::isfinite(slot.end_seconds))throw OcdfgPerformanceError("ValueError","business-hour slot endpoints must be finite");
 std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tie(a.start_seconds,a.end_seconds)<std::tie(b.start_seconds,b.end_seconds);});
 std::vector<BusinessHourSlot> merged;
 for(const auto& slot:result){if(!merged.empty()&&slot.start_seconds<=merged.back().end_seconds+1.0)merged.back().end_seconds=std::max(merged.back().end_seconds,slot.end_seconds);else merged.push_back(slot);}
 return merged;
}
std::int64_t wall_microseconds(const ParsedExactTimestamp& value){return value.seconds*1000000+static_cast<std::int64_t>(value.offset_minutes)*60000000+value.nanoseconds/1000;}
double elapsed(const ParsedExactTimestamp& a,const ParsedExactTimestamp& b){return ocdfg_elapsed_seconds(a,b);}
bool has_column(const Ocel& log,const std::string& key){
 // Round FD: a frame column no kept row holds (OcelFrameHint: an OCEL CSV
 // column of empty cells, a dropped row's column).
 const auto& hint=log.event_frame_hint;
 if(std::find(hint.columns.begin(),hint.columns.end(),key)!=hint.columns.end())return true;
 for(const auto& column:hint.dropped)if(column.key==key)return true;
 if(log.table_columns.events)return std::find(log.table_columns.events->begin(),log.table_columns.events->end(),key)!=log.table_columns.events->end();
 if(key=="ocel:eid"||key=="ocel:activity"||key=="ocel:timestamp")return true;
 for(const auto& event:log.events)for(const auto& attribute:event.attributes)if(attribute.key==key)return true;
 for(const auto& [type,schema]:log.event_type_attributes){(void)type;if(schema.contains(key))return true;}
 return false;
}
const OcelValue* attribute(const OcelEvent& event,const std::string& key){for(const auto& item:event.attributes)if(item.key==key)return &item.value;return nullptr;}

std::optional<std::string> selected_string(const OcelEvent& event,
                                         const std::string& key) {
 if(key=="ocel:eid")return event.id;
 if(key=="ocel:activity")return event.activity;
 if(key=="ocel:timestamp")return event.timestamp;
 const auto* value=attribute(event,key);
 if(const auto* text=value?std::get_if<std::string>(value):nullptr)return *text;
 return std::nullopt;
}

bool datetime_column(const Ocel& log,const OcelEvent& event,
                     const std::string& key) {
 if(key=="ocel:timestamp")return true;
 if(key=="ocel:eid"||key=="ocel:activity")return false;
 // Round EZ: a text the reference keeps as a str (OcelAttribute::text).
 for(const auto& attribute:event.attributes)if(attribute.key==key&&attribute.text)return false;
 const auto type=log.event_type_attributes.find(event.activity);
 if(type==log.event_type_attributes.end())return false;
 const auto field=type->second.find(key);
 return field!=type->second.end()&&
        (field->second.find("date")!=std::string::npos||
         field->second.find("time")!=std::string::npos);
}
// Round FD: an events attribute column as the time column. The reference
// subtracts and replace()s the frame's Python cells: in a datetime64 column
// Timestamps (a missing cell NaT, as the canonical column), in any other
// column the cells as they are (a date-read text a datetime.datetime, a
// missing cell a float NaN, an explicit null None, numbers int, float or
// bool by the column's dtype; a str column holds a null as NaN too): a pair
// of two numbers measures 0, a pair of anything else but two datetimes
// raises with the operands' type names (observed black-box). A repeated
// identifier times by its first row's cell, null or not.
struct TimeCell {
 enum class Kind { kDate, kNaT, kStr, kInt, kFloat, kBool, kNone } kind = Kind::kNaT;
 ParsedExactTimestamp date{};
};
const char* python_type(const TimeCell& cell){
 switch(cell.kind){
  case TimeCell::Kind::kDate:return "datetime.datetime";
  case TimeCell::Kind::kStr:return "str";
  case TimeCell::Kind::kInt:return "int";
  case TimeCell::Kind::kBool:return "bool";
  case TimeCell::Kind::kNone:return "NoneType";
  default:return "float";
 }
}
bool number(const TimeCell& cell){return cell.kind==TimeCell::Kind::kInt||cell.kind==TimeCell::Kind::kFloat||cell.kind==TimeCell::Kind::kBool;}
TimeCell time_cell(const Ocel& log,const OcelEvent& event,const std::string& key,ocel_frame::Dtype dtype,bool str_column,bool csv){
 using Kind=TimeCell::Kind;using ocel_frame::Dtype;
 TimeCell cell;
 if(csv){
  for(const auto& attribute:event.attributes)
   if(attribute.key==key&&!std::holds_alternative<std::nullptr_t>(attribute.value)){cell.kind=Kind::kStr;return cell;}
  cell.kind=Kind::kFloat;return cell;
 }
 const OcelAttribute* found=nullptr;
 for(const auto& attribute:event.attributes)if(attribute.key==key){found=&attribute;break;}
 const bool dates=dtype==Dtype::kDatetime;
 if(!found){cell.kind=dates?Kind::kNaT:Kind::kFloat;return cell;}
 const auto& value=found->value;
 if(std::holds_alternative<std::nullptr_t>(value)){cell.kind=dates?Kind::kNaT:dtype==Dtype::kFloat64||str_column?Kind::kFloat:Kind::kNone;return cell;}
 if(const auto* text=std::get_if<std::string>(&value)){
  if(ocel_frame::reference_date_value(log.event_type_attributes,event.activity,*found)){
   if(const auto parsed=parse_timestamp_exact(*text)){cell.kind=Kind::kDate;cell.date=*parsed;return cell;}
  }
  cell.kind=Kind::kStr;return cell;
 }
 if(std::holds_alternative<bool>(value)){cell.kind=Kind::kBool;return cell;}
 if(std::holds_alternative<std::int64_t>(value)){cell.kind=dtype==Dtype::kFloat64?Kind::kFloat:Kind::kInt;return cell;}
 cell.kind=Kind::kFloat;return cell;
}
std::vector<double> attribute_performance(const Ocel& log,const std::vector<std::pair<std::string,std::string>>& couples,
                                          const OcdfgEdgePerformanceOptions& options,const std::string& id_key,const std::string& time_key){
 using Kind=TimeCell::Kind;
 const auto frame=ocel_frame::event_frame(log,true);
 const auto dtype=frame.dtype(time_key);
 const auto* stats=frame.stats(time_key);
 // An OCEL CSV read's columns are str ones (OcelFrameHint::str_columns).
 const bool csv=log.event_frame_hint.str_columns;
 const bool str_column=csv||(dtype==ocel_frame::Dtype::kObject&&stats&&stats->has_string&&!stats->has_bool&&!stats->has_int&&
                       !stats->has_float&&!stats->has_date()&&!stats->has_none);
 std::map<std::string,TimeCell> cells;
 for(const auto& source:log.events){
  const auto identifier=selected_string(source,id_key);
  if(!identifier||cells.count(*identifier))continue;
  cells.emplace(*identifier,time_cell(log,source,time_key,dtype,str_column,csv));
 }
 for(const auto& [from,to]:couples){if(!cells.count(from))missing(from);if(!cells.count(to))missing(to);}
 // The pairs measure in the order given (see finish_ocdfg_edges).
 std::optional<std::vector<BusinessHourSlot>> schedule;
 std::vector<double> result;result.reserve(couples.size());
 for(const auto& [from,to]:couples){
  const TimeCell& a=cells.at(from);const TimeCell& b=cells.at(to);
  if(options.business_hours){
   for(const TimeCell* side:{&a,&b}){
    if(side->kind==Kind::kStr)throw OcdfgPerformanceError("TypeError","str.replace() takes no keyword arguments");
    if(side->kind!=Kind::kDate&&side->kind!=Kind::kNaT)throw OcdfgPerformanceError("AttributeError",std::string("'")+python_type(*side)+"' object has no attribute 'replace'");
   }
   if(a.kind==Kind::kNaT||b.kind==Kind::kNaT)throw OcdfgPerformanceError("ValueError","cannot convert float NaN to integer");
   if(!schedule)schedule=slots(options.business_hour_slots);
   result.push_back(detail::business_elapsed_seconds_microseconds(wall_microseconds(a.date),wall_microseconds(b.date),*schedule));
   continue;
  }
  if((a.kind==Kind::kDate||a.kind==Kind::kNaT)&&(b.kind==Kind::kDate||b.kind==Kind::kNaT)){
   if(a.kind==Kind::kNaT||b.kind==Kind::kNaT){result.push_back(0.0);continue;}
   if(a.date.timezone_aware!=b.date.timezone_aware)throw OcdfgPerformanceError("TypeError","can't subtract offset-naive and offset-aware datetimes");
   result.push_back(elapsed(a.date,b.date));continue;
  }
  if(number(a)&&number(b)){result.push_back(0.0);continue;}
  throw OcdfgPerformanceError("TypeError",std::string("unsupported operand type(s) for -: '")+python_type(b)+"' and '"+python_type(a)+"'");
 }
 std::sort(result.begin(),result.end());return result;
}
} // namespace
std::vector<double> calculate_ocdfg_edge_performance(const std::vector<OcdfgPerformanceEvent>& events,const std::vector<std::pair<std::string,std::string>>& couples,const OcdfgEdgePerformanceOptions& options){
 // Round FD: a repeated event identifier (an event id column whose values
 // repeat) times by its first event, as the reference's groupby first().
 std::map<std::string,const OcdfgPerformanceTimestamp*> times;for(const auto& event:events)times.try_emplace(event.id,&event.timestamp);
 // Empty aggregation entries don't enter calendar/slot processing.
 std::optional<std::vector<BusinessHourSlot>> schedule;
 std::vector<double> result;result.reserve(couples.size());
 for(const auto& [from,to]:couples){const auto left=times.find(from),right=times.find(to);if(left==times.end())missing(from);if(right==times.end())missing(to);
  const auto* a=std::get_if<ParsedExactTimestamp>(left->second);const auto* b=std::get_if<ParsedExactTimestamp>(right->second);
  if(options.business_hours){
   if(std::holds_alternative<std::string>(*left->second)||std::holds_alternative<std::string>(*right->second))throw OcdfgPerformanceError("TypeError","str.replace() takes no keyword arguments");
   if(!a||!b)throw OcdfgPerformanceError("ValueError","cannot convert float NaN to integer");
   if(!schedule)schedule=slots(options.business_hour_slots);
   result.push_back(detail::business_elapsed_seconds_microseconds(wall_microseconds(*a),wall_microseconds(*b),*schedule));
  }else{
   if(std::holds_alternative<std::string>(*left->second)||std::holds_alternative<std::string>(*right->second))throw OcdfgPerformanceError("TypeError","unsupported operand type(s) for -: 'str' and 'str'");
   result.push_back(a&&b?elapsed(*a,*b):0.0);
  }
 }
 std::sort(result.begin(),result.end());return result;
}
std::vector<double> calculate_ocdfg_edge_performance(const Ocel& log,const std::vector<std::pair<std::string,std::string>>& couples,const OcdfgEdgePerformanceOptions& options){
 const auto id_key=options.event_id_attribute.value_or("ocel:eid"),time_key=options.timestamp_attribute.value_or("ocel:timestamp");
 if(!has_column(log,id_key))missing(id_key);if(!has_column(log,time_key))missing(time_key,true);
 // Round EH: a time column of None (Ocel::none_event_times) fails on the
 // first pair it measures: the difference of two None, or their replace()
 // for business hours (observed black-box).
 if(time_key=="ocel:timestamp"&&log.none_event_times&&!couples.empty()){
  if(options.business_hours)throw OcdfgPerformanceError("AttributeError","'NoneType' object has no attribute 'replace'");
  throw OcdfgPerformanceError("TypeError","unsupported operand type(s) for -: 'NoneType' and 'NoneType'");
 }
 // Round EL: so does a raw time column (OcelRawTime) on its first pair:
 // the difference of the two Python values (a str in it raises TypeError,
 // two numbers have no total_seconds()), or the replace() of the first
 // value for business hours (observed black-box).
 if(time_key=="ocel:timestamp"&&!couples.empty()&&ocel_raw_event_times(log)){
  std::map<std::string_view,OcelRawTime> kinds;for(const auto& event:log.events)kinds.emplace(event.id,event.raw_time);
  const auto kind=[&](const std::string& id){const auto found=kinds.find(id);if(found==kinds.end())missing(id);return found->second;};
  throw ocdfg_raw_pair_error(kind(couples.front().first),kind(couples.front().second),options.business_hours);
 }
 if(time_key!="ocel:timestamp"&&time_key!="ocel:eid"&&time_key!="ocel:activity")
  return attribute_performance(log,couples,options,id_key,time_key);
 std::vector<OcdfgPerformanceEvent> events;events.reserve(log.events.size());
 for(const auto& source:log.events){OcdfgPerformanceEvent event;event.id=source.id;
  const auto identifier=selected_string(source,id_key);
  if(!identifier)continue;
  event.id=*identifier;
  const auto text=selected_string(source,time_key);
  if(text){
   const bool typed_datetime=datetime_column(log,source,time_key);
   if(typed_datetime&&(text->empty()||*text=="NaT"))event.timestamp=std::monostate{};
   else if(!typed_datetime)event.timestamp=*text;
   else if(const auto timestamp=parse_timestamp_exact(*text))event.timestamp=*timestamp;
   else event.timestamp=*text;
  }
  events.push_back(std::move(event));
 }
 return calculate_ocdfg_edge_performance(events,couples,options);
}
OcdfgPerformanceError ocdfg_raw_pair_error(OcelRawTime from,OcelRawTime to,bool business_hours){
 const std::string from_type=ocel_raw_time_type(from),to_type=ocel_raw_time_type(to);
 if(business_hours){
  if(from==OcelRawTime::kStr)return OcdfgPerformanceError("TypeError","str.replace() takes no keyword arguments");
  return OcdfgPerformanceError("AttributeError","'"+from_type+"' object has no attribute 'replace'");
 }
 // Round EN: None (OcelRawTime::kNull) raises as a str does.
 if(from==OcelRawTime::kStr||to==OcelRawTime::kStr||from==OcelRawTime::kNull||to==OcelRawTime::kNull)
  return OcdfgPerformanceError("TypeError","unsupported operand type(s) for -: '"+to_type+"' and '"+from_type+"'");
 return OcdfgPerformanceError("AttributeError",std::string("'")+(from==OcelRawTime::kInt&&to==OcelRawTime::kInt?"int":"float")+"' object has no attribute 'total_seconds'");
}
double ocdfg_elapsed_seconds(const ParsedExactTimestamp& a,const ParsedExactTimestamp& b){
 // Floor the nanosecond difference to microseconds, split it into days,
 // seconds and microseconds with nonnegative remainders, then sum in that
 // order. Whole days and seconds stay exact; only the fraction rounds.
 const auto nanos=static_cast<std::int64_t>(b.nanoseconds)-a.nanoseconds;
 auto fraction=nanos/1000;if(nanos<0&&nanos%1000)--fraction;
 const std::int64_t micros=(b.seconds-a.seconds)*1000000+fraction;
 constexpr std::int64_t per_day=86400LL*1000000;
 auto days=micros/per_day,rest=micros%per_day;if(rest<0){rest+=per_day;--days;}
 const auto whole=days*86400+rest/1000000;
 return static_cast<double>(whole)+static_cast<double>(rest%1000000)/1e6;
}
} // namespace minera
