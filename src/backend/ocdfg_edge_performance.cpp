#include "minera/ocdfg_edge_performance.hpp"
#include "business_hours.hpp"
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
 const auto type=log.event_type_attributes.find(event.activity);
 if(type==log.event_type_attributes.end())return false;
 const auto field=type->second.find(key);
 return field!=type->second.end()&&
        (field->second.find("date")!=std::string::npos||
         field->second.find("time")!=std::string::npos);
}
} // namespace
std::vector<double> calculate_ocdfg_edge_performance(const std::vector<OcdfgPerformanceEvent>& events,const std::vector<std::pair<std::string,std::string>>& couples,const OcdfgEdgePerformanceOptions& options){
 std::map<std::string,const OcdfgPerformanceTimestamp*> times;for(const auto& event:events)times[event.id]=&event.timestamp;
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
