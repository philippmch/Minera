#include "minera/reference_log_filter_workbench.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/log_boundary_filter.hpp"
#include "minera/log_between_filter.hpp"
#include "minera/log_case_filter.hpp"
#include "minera/log_paths_filter.hpp"
#include "minera/log_timestamp_filter.hpp"
#include "minera/log_variants_filter.hpp"
#include "minera/variants_filter_controls.hpp"
#include "minera/paths_filter_controls.hpp"
#include "minera/case_filter_controls.hpp"
#include "minera/log_rework_filter.hpp"
#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/log_segments_filter.hpp"
#include "minera/log_trace_filter.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {
std::string q(std::string_view s) { return serialize_embedding_workbench_string_json(s); }
std::string counts(const EventLog& log) {
  std::size_t events = 0;
  for (const auto& trace : log.traces) events += trace.events.size();
  return "{\"traces\":"+std::to_string(log.traces.size())+",\"events\":"+std::to_string(events)+'}';
}
std::vector<std::vector<std::string>> patterns(std::string_view raw) {
  std::size_t at = 0;
  const auto space = [&] { while (at < raw.size() &&
      (raw[at]==' ' || raw[at]=='\t' || raw[at]=='\r' || raw[at]=='\n')) ++at; };
  const auto require = [&](char c) {
    space(); if (at==raw.size() || raw[at++]!=c)
      throw std::invalid_argument("patterns_json must be an array of activity arrays");
  };
  require('['); space();
  std::vector<std::vector<std::string>> result;
  if (at < raw.size() && raw[at]==']') ++at;
  else while (true) {
    space(); const auto begin = at; require('[');
    std::size_t depth = 1; bool quoted = false, escaped = false;
    while (at < raw.size() && depth) {
      const char c = raw[at++];
      if (quoted) { if (escaped) escaped=false; else if(c=='\\') escaped=true;
        else if(c=='"') quoted=false; }
      else if(c=='"') quoted=true;
      else if(c=='[') ++depth;
      else if(c==']') --depth;
    }
    if (depth) throw std::invalid_argument("patterns_json has an incomplete activity array");
    result.push_back(parse_literal_string_vector_json(raw.substr(begin,at-begin)));
    space();
    if (at < raw.size() && raw[at]==']') { ++at; break; }
    require(',');
  }
  space(); if (at!=raw.size()) throw std::invalid_argument("patterns_json has trailing content");
  return result;
}
EventAttribute endpoint(std::string_view raw) {
  const auto at=raw.find_first_not_of(" \r\n\t");
  if(at!=std::string_view::npos && raw[at]=='"') {
    const auto values=parse_literal_string_vector_json("["+std::string(raw)+"]");
    if(values.size()!=1) throw std::invalid_argument("between endpoint requires one string or an array of strings");
    return EventAttribute{"",values.front()};
  }
  EventAttribute result{"",""};result.container=AttributeContainerKind::kList;
  const auto values=parse_literal_string_vector_json(raw);
  for(std::size_t i=0;i<values.size();++i) result.children.push_back({std::to_string(i),values[i]});
  return result;
}
LogReworkMinimum minimum(std::string_view raw) {
  if(raw=="null") return std::monostate{};
  if(raw=="true") return true;
  if(raw=="false") return false;
  if(raw=="NaN") return std::numeric_limits<double>::quiet_NaN();
  if(raw=="INF" || raw=="+INF") return std::numeric_limits<double>::infinity();
  if(raw=="-INF") return -std::numeric_limits<double>::infinity();
  if(!raw.empty() && raw.front()=='"') {
    const auto strings=parse_literal_string_vector_json("["+std::string(raw)+"]");
    if(strings.size()!=1) throw std::invalid_argument("minimum requires one scalar");
    return strings.front();
  }
  if(raw.empty()) throw std::invalid_argument("min_occurrences requires a numeric scalar");
  if(raw.find_first_of(".eE")==std::string_view::npos) {
    if(raw.front()=='-') {
      std::int64_t value=0;const auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),value);
      if(parsed.ec==std::errc{} && parsed.ptr==raw.data()+raw.size()) return value;
    } else {
      std::uint64_t value=0;const auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),value);
      if(parsed.ec==std::errc{} && parsed.ptr==raw.data()+raw.size()) return value;
    }
    const auto digits=raw.substr(raw.front()=='-'?1:0);
    if(!digits.empty() && std::all_of(digits.begin(),digits.end(),[](char c){return c>='0' && c<='9';}))
      // Beyond either exact carrier's range the comparison with every possible
      // uint64 event count is fixed. Preserve that result without rounding.
      return raw.front()=='-'?-std::numeric_limits<double>::infinity():std::numeric_limits<double>::infinity();
  }
  double value=0;const auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),value);
  if(parsed.ec!=std::errc{} || parsed.ptr!=raw.data()+raw.size())
    throw std::invalid_argument("min_occurrences requires an integer, number, NaN, INF or -INF");
  return value;
}
} // namespace

DiscoveryWorkbenchResponse execute_reference_log_filter(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error) {
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;
  std::optional<EventLog> source,result;
  std::optional<std::string> source_before;
  std::string operation,format,key,open_key,case_key,full_result,phase="control",error,export_error,raw_result,xes,
      time_key="time:timestamp", time_attribute_key="concept:name", source_after;
  std::optional<bool> unchanged;
  int status=200;
  const auto field=[&](const std::string& name,std::string fallback={}) {
    const auto found=fields.find(name);return found==fields.end()?fallback:found->second->bytes;
  };
  const auto boolean=[&](const std::string& name,bool fallback) {
    if(!fields.contains(name)) return fallback;
    const auto value=field(name);
    if(value!="true" && value!="false") throw std::invalid_argument(name+" must be true or false");
    return value=="true";
  };
  try {
    if(!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed={"operation","source_file","source_format",
      "activity_key","activity","positive","strict","occurrence","admitted_json","patterns_json",
      "min_occurrences","target_json","min_rep","max_rep","act1_json","act2_json","case_id_key","subcase_concat_str","case_id_column","activity_column","timestamp_column","delimiter","trim_fields",
      "minimum","maximum","max_cases","timestamp_key","paths_json","path_json",
      "lower","upper","time_attribute_key","time_target_json","criterion","sequence_kind"};
    for(const auto* collection:{&query,&parts}) for(const auto& part:*collection) {
      if(!allowed.contains(part.name)) throw std::invalid_argument("unknown log filter control: "+part.name);
      if(!fields.emplace(part.name,&part).second) throw std::invalid_argument("duplicate log filter control: "+part.name);
    }
    static const std::set<std::string> operations={"traces","prefix","suffix","start","end","rework","repetition","between","case_size","case_count","case_performance","paths","path_performance",
      "time_events","time_contained","time_intersecting","time_starting","time_completing","time_attribute","time_trace_attribute",
      "variants","variants_top_k","variants_min_coverage","variants_max_coverage","variants_cumulative"};
    operation=field("operation");
    if(!operations.contains(operation)) { operation.clear();throw std::invalid_argument("choose a supported log filter"); }
    const auto found=fields.find("source_file");
    if(found==fields.end() || !found->second->file)
      throw std::invalid_argument("source_file requires an uploaded log File");
    for(const auto& part:query) if(part.name=="source_file")
      throw std::invalid_argument("source_file must be a multipart File");
    const bool segment=operation=="prefix" || operation=="suffix";
    const bool boundary=operation=="start" || operation=="end";
    const bool repetition=operation=="repetition";
    const bool between=operation=="between";
    const bool variants=operation=="variants" || operation.starts_with("variants_");
    if(fields.contains("criterion")!=(variants && operation!="variants")) throw std::invalid_argument("criterion requires ranked or coverage variants filtering");
    if(fields.contains("sequence_kind") && operation!="variants") throw std::invalid_argument("sequence_kind requires variants membership filtering");
    const auto sequence_kind=field("sequence_kind","list");
    if(sequence_kind!="list" && sequence_kind!="tuple") throw std::invalid_argument("sequence_kind must be list or tuple");
    if(operation=="variants" && !fields.contains("admitted_json")) throw std::invalid_argument("variants filtering requires admitted_json");
    if(operation=="variants_cumulative" && fields.contains("positive")) throw std::invalid_argument("positive is inapplicable to cumulative variants filtering");
    const bool cases=operation=="case_size" || operation=="case_count" || operation=="case_performance";
    const bool path_performance=operation=="path_performance", paths=operation=="paths" || path_performance;
    const bool time=operation.starts_with("time_"), time_boundary=operation=="time_starting" || operation=="time_completing";
    if(time && (!fields.contains("lower") || !fields.contains("upper"))) throw std::invalid_argument("timestamp filtering requires lower and upper endpoints");
    if(!time && (fields.contains("lower") || fields.contains("upper"))) throw std::invalid_argument("lower/upper require timestamp filtering");
    if(operation!="time_attribute" && (fields.contains("time_attribute_key") || fields.contains("time_target_json"))) throw std::invalid_argument("time attribute controls require time_attribute filtering");
    if(operation=="time_attribute" && !fields.contains("time_target_json")) throw std::invalid_argument("time_attribute requires time_target_json");
    if(time && !time_boundary && fields.contains("positive")) throw std::invalid_argument("positive requires starting/completing timestamp filtering");
    if(paths && !fields.contains(path_performance?"path_json":"paths_json")) throw std::invalid_argument(path_performance?"path performance filtering requires path_json":"paths filtering requires paths_json");
    if(paths && fields.contains(path_performance?"paths_json":"path_json")) throw std::invalid_argument("choose only the active path control");
    if(!paths && (fields.contains("paths_json") || fields.contains("path_json"))) throw std::invalid_argument("paths_json/path_json require path filtering");
    if(cases && fields.contains("activity_key")) throw std::invalid_argument("activity_key is inapplicable to case filtering");
    if(!cases && !path_performance && (fields.contains("minimum") || fields.contains("maximum") || fields.contains("max_cases")))
      throw std::invalid_argument("bounds require case or path performance filtering");
    if(cases && fields.contains("positive")) throw std::invalid_argument("positive is inapplicable to case filtering");
    if(operation=="case_count" && (fields.contains("minimum") || fields.contains("maximum"))) throw std::invalid_argument("minimum/maximum require size or performance filtering");
    if(operation!="case_count" && fields.contains("max_cases")) throw std::invalid_argument("max_cases requires case_count filtering");
    if(operation!="case_performance" && !path_performance && !time && fields.contains("timestamp_key")) throw std::invalid_argument("timestamp_key requires timestamp, case or path performance filtering");
    if(between && (!fields.contains("act1_json") || !fields.contains("act2_json")))
      throw std::invalid_argument("between filtering requires act1_json and act2_json");
    if(!between && (fields.contains("act1_json") || fields.contains("act2_json") || fields.contains("case_id_key") || fields.contains("subcase_concat_str")))
      throw std::invalid_argument("act1_json/act2_json/case_id_key/subcase_concat_str require between filtering");
    if(between && fields.contains("positive")) throw std::invalid_argument("positive is inapplicable to between filtering");
    if(repetition && !fields.contains("target_json"))
      throw std::invalid_argument("attribute repetition requires target_json");
    if(!repetition && (fields.contains("target_json") || fields.contains("min_rep") || fields.contains("max_rep")))
      throw std::invalid_argument("target_json/min_rep/max_rep require attribute repetition filtering");
    if(repetition && fields.contains("positive"))
      throw std::invalid_argument("positive is inapplicable to attribute repetition filtering");
    if((segment || operation=="rework") && !fields.contains("activity"))
      throw std::invalid_argument("this filter requires an activity (an empty label is valid)");
    if(operation=="traces" && !fields.contains("patterns_json"))
      throw std::invalid_argument("trace filtering requires patterns_json");
    if(boundary && !fields.contains("admitted_json"))
      throw std::invalid_argument("start/end filtering requires admitted_json");
    if(!segment && (fields.contains("strict") || fields.contains("occurrence")))
      throw std::invalid_argument("strict/occurrence controls require prefix or suffix filtering");
    if(segment && fields.contains("positive")) throw std::invalid_argument("positive is inapplicable to prefix/suffix filtering");
    if(operation!="rework" && fields.contains("min_occurrences"))
      throw std::invalid_argument("min_occurrences requires rework filtering");
    if(!boundary && operation!="variants" && fields.contains("admitted_json")) throw std::invalid_argument("admitted_json requires start/end or variants membership filtering");
    if(operation!="traces" && fields.contains("patterns_json")) throw std::invalid_argument("patterns_json requires trace filtering");
    if(!(segment || operation=="rework") && fields.contains("activity"))
      throw std::invalid_argument("activity requires prefix, suffix or rework filtering");
    const auto& file=*found->second;
    format=field("source_format","auto");
    if(format=="auto") {
      auto name=file.filename;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
      const auto first=file.bytes.find_first_not_of(" \r\n\t");
      if(name.ends_with(".csv")) format="csv";
      else if(name.ends_with(".json")) format="native_log";
      else if(name.ends_with(".xes") || name.ends_with(".xes.gz")) format="xes";
      else format=first!=std::string::npos && file.bytes[first]=='{'?"native_log":
          first!=std::string::npos && file.bytes[first]=='<'?"xes":"csv";
    }
    if(format!="native_log" && format!="csv" && format!="xes") { format.clear();throw std::invalid_argument("source_format must be auto, csv, xes or native_log"); }
    key=field("activity_key",format=="csv"?field("activity_column","activity"):"concept:name");
    if(time) {
      time_key=field("timestamp_key",operation=="time_trace_attribute"?"time:timestamp":format=="csv"?field("timestamp_column","timestamp"):"time:timestamp");
      time_attribute_key=field("time_attribute_key",format=="csv"?field("activity_column","activity"):"concept:name");
    }
    if(between) case_key=field("case_id_key",format=="csv"?field("case_id_column","case_id"):"concept:name");
    phase="source_import";
    if(format=="native_log") source=parse_embedding_workbench_raw_log_json(file.bytes);
    else {
      std::istringstream input(file.bytes);
      if(format=="xes") source=import_xes_event_log(input);
      else {
        CsvImportOptions options;options.case_id_column=field("case_id_column","case_id");
        options.activity_column=field("activity_column","activity");options.timestamp_column=field("timestamp_column","timestamp");
        options.trim_fields=boolean("trim_fields",false);options.allow_case_id_aliases=true;
        const auto delimiter=field("delimiter",",");
        if(delimiter.size()!=1) throw std::invalid_argument("CSV delimiter must contain exactly one byte");
        options.delimiter=delimiter.front();source=import_csv_event_log(input,options);
      }
    }
    open_key=key;
    if(repetition || between || cases || paths || time || variants) {
      open_key=format=="csv"?field("activity_column","activity"):"concept:name";
      if(format!="csv") {
        bool found_activity=false;
        for(const auto& trace:source->traces) {
          for(const auto& event:trace.events) {
            if(!event.activity_source_key.empty()) { open_key=event.activity_source_key; found_activity=true; break; }
            if(event.activity_attribute) { open_key=event.activity_attribute->key; found_activity=true; break; }
          }
          if(found_activity) break;
        }
        // A custom-key XES projection can have no first-class activity role.
        // Its explicitly chosen path attribute supplies the opening role.
        if(!found_activity && (paths || time || variants) && fields.contains("activity_key")) open_key=key;
      }
    }
    if(operation=="time_events" && format=="csv") {
      // Prepare the conventional EventLog identity required by the public
      // event-to-log converter while preserving the original CSV case role.
      for(auto& trace:source->traces) if(trace.case_id_source_key!="concept:name" &&
          std::none_of(trace.attributes.begin(),trace.attributes.end(),[](const EventAttribute& value){return value.key=="concept:name";})) {
        EventAttribute canonical=trace.case_id_attribute.value_or(EventAttribute{"",trace.case_id});
        canonical.key="concept:name";trace.attributes.push_back(std::move(canonical));
      }
    }
    source_before=serialize_embedding_workbench_raw_log_json(*source);
    phase="filter";
    if(variants) {
      LogVariantsFilterOptions options;options.activity_key=EventAttribute{"",key};options.positive=parse_case_filter_bound(boolean("positive",true)?"true":"false");
      if(operation=="variants") result=filter_log_variants(*source,parse_variants_filter_admitted_json(field("admitted_json"),sequence_kind=="tuple"),options);
      else {
        const auto criterion=parse_case_filter_bound(field("criterion"));
        if(operation=="variants_top_k") result=filter_log_variants_top_k(*source,criterion,options);
        else if(operation=="variants_min_coverage") result=filter_log_variants_min_coverage(*source,criterion,options);
        else if(operation=="variants_max_coverage") result=filter_log_variants_max_coverage(*source,criterion,options);
        else result=filter_log_variants_cumulative(*source,criterion,options);
      }
    } else if(time) {
      LogTimestampFilterOptions options; options.timestamp_key=EventAttribute{"",time_key};
      options.positive=parse_case_filter_bound(boolean("positive",true)?"true":"false");
      const EventAttribute lower{"",field("lower")},upper{"",field("upper")};
      if(operation=="time_events") result=filter_log_timestamp_events(*source,lower,upper,options);
      else if(operation=="time_contained") result=filter_log_timestamp_contained(*source,lower,upper,options);
      else if(operation=="time_intersecting") result=filter_log_timestamp_intersecting(*source,lower,upper,options);
      else if(operation=="time_starting") result=filter_log_timestamp_starting(*source,lower,upper,options);
      else if(operation=="time_completing") result=filter_log_timestamp_completing(*source,lower,upper,options);
      else if(operation=="time_trace_attribute") result=filter_log_timestamp_trace_attribute(*source,lower,upper,options);
      else result=filter_log_timestamp_attribute(*source,EventAttribute{"",time_attribute_key},parse_log_attribute_repetition_target_json(field("time_target_json")),lower,upper,options);
    } else if(paths) {
      if(path_performance) {
        LogPathsPerformanceOptions options;options.attribute_key=EventAttribute{"",key};options.positive=parse_case_filter_bound(boolean("positive",true)?"true":"false");
        options.timestamp_key=EventAttribute{"",field("timestamp_key",format=="csv"?field("timestamp_column","timestamp"):"time:timestamp")};
        if(fields.contains("minimum")) options.minimum=parse_case_filter_bound(field("minimum"));
        if(fields.contains("maximum")) options.maximum=parse_case_filter_bound(field("maximum"));
        result=filter_log_paths_performance(*source,parse_paths_filter_pair_json(field("path_json")),options);
      } else {
        LogPathsFilterOptions options;options.attribute_key=EventAttribute{"",key};options.positive=parse_case_filter_bound(boolean("positive",true)?"true":"false");
        result=filter_log_paths(*source,parse_paths_filter_pairs_json(field("paths_json")),options);
      }
    } else if(operation=="case_size") result=filter_log_on_case_size(*source,parse_case_filter_bound(field("minimum","2")),parse_case_filter_bound(field("maximum","null")));
    else if(operation=="case_count") result=filter_log_on_ncases(*source,parse_case_filter_bound(field("max_cases","1000")));
    else if(operation=="case_performance") {
      LogCasePerformanceOptions options; options.timestamp_key=EventAttribute{"",field("timestamp_key",format=="csv"?field("timestamp_column","timestamp"):"time:timestamp")};
      result=filter_log_on_case_performance(*source,parse_case_filter_bound(field("minimum","0")),parse_case_filter_bound(field("maximum","10000000000")),options);
    } else if(between) {
      LogBetweenFilterOptions options;options.activity_key=EventAttribute{"",key};options.case_id_key=EventAttribute{"",case_key};
      if(fields.contains("subcase_concat_str")) options.subcase_separator=EventAttribute{"",field("subcase_concat_str")};
      const auto complete=filter_log_between_result(*source,endpoint(field("act1_json")),endpoint(field("act2_json")),options);
      full_result=serialize_log_between_filter_result_json(complete);result=complete.project_event_log();
    } else if(operation=="traces") result=filter_log_traces(*source,patterns(field("patterns_json")),{boolean("positive",true),key});
    else if(boundary) {
      const auto admitted=parse_literal_string_vector_json(field("admitted_json"));
      LogBoundaryFilterOptions options{key,boolean("positive",true)};
      result=operation=="start"?filter_log_start_activities(*source,admitted,options):filter_log_end_activities(*source,admitted,options);
    } else if(segment) {
      LogSegmentFilterOptions options;options.activity_key=key;options.strict=boolean("strict",true);
      const auto occurrence=field("occurrence","first");
      if(occurrence!="first" && occurrence!="last") throw std::invalid_argument("occurrence must be first or last");
      options.occurrence=occurrence=="first"?LogSegmentOccurrence::first:LogSegmentOccurrence::last;
      result=operation=="prefix"?filter_log_prefix(*source,field("activity"),options):filter_log_suffix(*source,field("activity"),options);
    } else if(repetition) {
      LogAttributeRepetitionOptions options;options.attribute_key=key;
      if(fields.contains("min_rep")) options.minimum=parse_log_attribute_repetition_bound_json(field("min_rep"));
      if(fields.contains("max_rep")) options.maximum=parse_log_attribute_repetition_bound_json(field("max_rep"));
      result=filter_log_attribute_value_repetition(*source,parse_log_attribute_repetition_target_json(field("target_json")),options);
    } else {
      LogReworkFilterOptions options;options.activity_key=key;options.positive=boolean("positive",true);
      if(fields.contains("min_occurrences")) options.min_occurrences=minimum(field("min_occurrences"));
      result=filter_log_rework(*source,field("activity"),options);
    }
    unchanged=*source_before==serialize_embedding_workbench_raw_log_json(*source);
    phase="result_export";raw_result=serialize_embedding_workbench_raw_log_json(*result);
    try { XesExportOptions options;options.preserve_nulls=true;xes=serialize_xes(*result,options); }
    catch(const std::exception& e) { export_error=e.what(); }
    phase="complete";
  } catch(const LogVariantsFilterError& e) {
    status=400;error=e.message;
    if(source && source_before) unchanged=*source_before==serialize_embedding_workbench_raw_log_json(*source);
  } catch(const LogTimestampFilterError& e) {
    status=400;error=e.message;
    if(source && source_before) unchanged=*source_before==serialize_embedding_workbench_raw_log_json(*source);
  } catch(const LogPathsFilterError& e) {
    status=400;error=e.message;
    if(source && source_before) unchanged=*source_before==serialize_embedding_workbench_raw_log_json(*source);
  } catch(const LogCaseFilterError& e) {
    status=400;error=e.message;
    if(source && source_before) unchanged=*source_before==serialize_embedding_workbench_raw_log_json(*source);
  } catch(const LogBetweenFilterError& e) {
    status=400;error=e.message;
    if(source && source_before) unchanged=*source_before==serialize_embedding_workbench_raw_log_json(*source);
  } catch(const std::exception& e) {
    status=400;error=e.what();
    if(source && source_before) try { unchanged=*source_before==serialize_embedding_workbench_raw_log_json(*source); }
      catch(const std::exception&) { unchanged.reset(); }
  }
  if(operation.starts_with("time_") && source) try {source_after=serialize_embedding_workbench_raw_log_json(*source);} catch(const std::exception&) {}
  const auto optional_text=[&](const std::string& text,bool present) {return present?q(text):std::string("null");};
  std::string json="{\"family\":\"REFERENCE_LOG_FILTER\",\"operation\":"+optional_text(operation,!operation.empty())+
    ",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+",\"source_format\":"+optional_text(format,!format.empty())+
    ",\"source\":"+(source?counts(*source):"null")+",\"result\":"+(result?counts(*result):"null")+
    ",\"source_unchanged\":"+(unchanged?(*unchanged?"true":"false"):"null")+
    ",\"activity_key\":"+optional_text((operation=="repetition" || operation=="between" || operation.starts_with("case_") || operation.starts_with("time_") || operation.starts_with("variants") || operation=="paths" || operation=="path_performance")?open_key:key,source.has_value())+
    ((operation=="repetition" || operation=="between" || operation.starts_with("variants") || operation=="paths" || operation=="path_performance")?",\"filter_attribute_key\":"+optional_text(key,source.has_value()):"")+
    (operation=="between"?",\"case_id_key\":"+optional_text(case_key,source.has_value())+",\"full_result_json\":"+optional_text(full_result,!full_result.empty()):"")+
    (operation.starts_with("time_")?",\"timestamp_key\":"+q(time_key)+",\"time_attribute_key\":"+q(time_attribute_key)+
      ",\"source_log_json\":"+(source_before?q(*source_before):"null")+",\"source_after_log_json\":"+optional_text(source_after,!source_after.empty()):"")+
    ",\"filtered_log_json\":"+optional_text(raw_result,!raw_result.empty())+
    ",\"filtered_xes\":"+optional_text(xes,result.has_value() && export_error.empty() && error.empty())+
    ",\"xes_error\":"+optional_text(export_error,!export_error.empty())+
    ",\"error\":"+optional_text(error,!error.empty())+'}';
  return {status,std::move(json)};
}
} // namespace minera
