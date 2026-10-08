import { logToFeaturesQuery,logToTargetQuery,labelSplittingQuery,type LogToFeaturesOptions,type LogToTargetOptions,type LabelSplittingOptions } from "./api";
import { isLogToFeaturesResult,isLogToTargetResult,isLabelSplittingResult,type LogToFeaturesResult,type LogToTargetResult,type LabelSplittingResult } from "./types";

export type TransformationMethod="features"|"targets"|"labels";
export type NativeSwitch="native"|"true"|"false";
export interface ExactList {mode:"native"|"list"|"empty";text:string;}
export const FEATURE_LISTS=[
  ["event_string_attributes","Event string attributes"],["event_numeric_attributes","Event numeric attributes"],
  ["case_string_attributes","Case string attributes"],["case_numeric_attributes","Case numeric attributes"],
  ["event_attributes","Generic event attributes"],["case_attributes","Generic case attributes"],
  ["event_successor_attributes","Succession attributes"],["feature_names","Explicit feature schema"],
] as const;
export const EXTRA_FEATURES=[
  ["enable_case_duration","Case duration"],["enable_times_from_first_occurrence","Times from first occurrence"],
  ["enable_times_from_last_occurrence","Times from last occurrence"],["enable_direct_paths_times_last_occ","Direct path times"],
  ["enable_indirect_paths_times_last_occ","Indirect path times"],["enable_work_in_progress","Work in progress"],
  ["enable_resource_workload","Resource workload"],["enable_first_last_activity_index","First and last activity indices"],
  ["enable_max_concurrent_events","Maximum concurrent events"],["enable_max_concurrent_events_per_activity","Concurrency per activity"],
] as const;
export const TEMPORAL_DESTINATIONS=[
  ["arrival_rate","Arrival rate field","@@arrival_rate"],["finish_rate","Finish rate field","@@finish_rate"],
  ["service_time","Service time field","@@service_time"],["waiting_time","Waiting time field","@@waiting_time"],
  ["sojourn_time","Sojourn time field","@@sojourn_time"],["diff_start_end","Start/end difference field","@@diff_start_end"],
] as const;
type FeatureListKey=typeof FEATURE_LISTS[number][0];
type ExtraKey=typeof EXTRA_FEATURES[number][0];
type DestinationKey=typeof TEMPORAL_DESTINATIONS[number][0];
export interface FeatureForm {
  variant:"event_based"|"trace_based"|"temporal";activity:string;timestamp:string;start:string;resource:string;caseId:string;
  grouping:"calendar"|"fixed";frequency:string;window:string;missing:string;epsilon:string;minimum:string;maximum:string;
  activityRepresentation:boolean;successionRepresentation:boolean;allExtra:boolean;caseIdentifier:boolean;
  lists:Record<FeatureListKey,ExactList>;extra:Record<ExtraKey,NativeSwitch>;destinations:Record<DestinationKey,string>;
}
export function defaultFeatureForm():FeatureForm {return {variant:"trace_based",activity:"concept:name",timestamp:"time:timestamp",start:"",resource:"org:resource",caseId:"",grouping:"calendar",frequency:"W",window:"604800",missing:"0",epsilon:"0.000001",minimum:"2",maximum:"500",activityRepresentation:true,successionRepresentation:true,allExtra:false,caseIdentifier:false,lists:Object.fromEntries(FEATURE_LISTS.map(([key])=>[key,{mode:"native",text:"[]"}])) as FeatureForm["lists"],extra:Object.fromEntries(EXTRA_FEATURES.map(([key])=>[key,"native"])) as FeatureForm["extra"],destinations:Object.fromEntries(TEMPORAL_DESTINATIONS.map(([key,,value])=>[key,value])) as FeatureForm["destinations"]};}
export interface TargetForm {variant:"next_activity"|"next_time"|"remaining_time";activity:string;timestamp:string;padding:boolean;size:string;activities:ExactList;}
export const DEFAULT_TARGET:TargetForm={variant:"next_activity",activity:"concept:name",timestamp:"time:timestamp",padding:false,size:"0",activities:{mode:"native",text:"[]"}};
export interface LabelForm {activity:string;target:string;prefix:string;suffix:string;weight:string;separator:string;activities:ExactList;}
export const DEFAULT_LABEL:LabelForm={activity:"concept:name",target:"concept:name",prefix:"2",suffix:"2",weight:"0",separator:"_",activities:{mode:"native",text:"[]"}};
export function exactList(list:ExactList,label:string):string[]|undefined {
  if(list.mode==="native")return undefined;if(list.mode==="empty")return [];
  let value:unknown;try{value=JSON.parse(list.text);}catch{throw new Error(`${label} must be a JSON array of exact names.`);}
  if(!Array.isArray(value)||!value.every(item=>typeof item==="string"))throw new Error(`${label} must be a JSON array of exact names.`);return value;
}
function finite(text:string,label:string,min=-Infinity,positive=false):number {const value=Number(text);if(!text.trim()||!Number.isFinite(value)||value<min||positive&&value===min)throw new Error(min===-Infinity?`${label} must be a finite number.`:`${label} must be finite and ${positive?"greater than": "at least"} ${min}.`);return value;}
function integer(text:string,label:string,min:number,max:number):number {const value=Number(text);if(!text.trim()||!Number.isSafeInteger(value)||value<min||value>max)throw new Error(`${label} must be an integer from ${min} through ${max}.`);return value;}
function required(value:string,label:string):string {if(!value)throw new Error(`${label} must not be empty.`);return value;}
export function featureOptions(form:FeatureForm):LogToFeaturesOptions {
  const options:LogToFeaturesOptions={features_variant:form.variant,features_activity_key:required(form.activity,"Activity attribute"),features_timestamp_key:required(form.timestamp,"Timestamp attribute"),features_resource_key:required(form.resource,"Resource attribute"),features_default_not_present:finite(form.missing,"Missing feature value"),features_epsilon:finite(form.epsilon,"Epsilon",0),features_min_num_different_string_values:integer(form.minimum,"Minimum distinct strings",0,Number.MAX_SAFE_INTEGER),features_max_num_different_string_values:integer(form.maximum,"Maximum distinct strings",0,Number.MAX_SAFE_INTEGER),features_enable_activity_def_representation:form.activityRepresentation,features_enable_succ_def_representation:form.successionRepresentation,features_enable_all_extra_features:form.allExtra,features_add_case_identifier_column:form.caseIdentifier};
  if(form.caseId)options.features_case_id_key=form.caseId;if(form.start)options.features_start_timestamp_key=form.start;
  if(form.variant==="temporal") {
    if(form.grouping==="calendar"){options.features_grouper_freq=required(form.frequency,"Calendar frequency");for(const [key] of TEMPORAL_DESTINATIONS)Object.assign(options,{["features_"+key]:form.destinations[key]});}
    else options.features_window_seconds=finite(form.window,"Window seconds",0,true);
  }
  for(const [key,label] of FEATURE_LISTS){const list=exactList(form.lists[key],label);if(list!==undefined)Object.assign(options,{["features_"+key]:list});}
  for(const [key] of EXTRA_FEATURES)if(form.extra[key]!=="native")Object.assign(options,{["features_"+key]:form.extra[key]==="true"});
  return options;
}
export function targetOptions(form:TargetForm):LogToTargetOptions {
  const options:LogToTargetOptions={target_variant:form.variant,target_activity_key:required(form.activity,"Activity attribute"),target_timestamp_key:required(form.timestamp,"Timestamp attribute"),target_enable_padding:form.padding,target_pad_size:form.padding?integer(form.size,"Padding rows per case",0,100000):0};
  if(form.variant==="next_activity"){const activities=exactList(form.activities,"Activity classes");if(activities!==undefined){if(!activities.length||activities.some(value=>!value)||new Set(activities).size!==activities.length)throw new Error("Activity classes must be non-empty unique names.");options.target_activities=activities;}}
  return options;
}
export function labelOptions(form:LabelForm):LabelSplittingOptions {
  const options:LabelSplittingOptions={label_splitting_variant:"contextual",label_splitting_activity_key:required(form.activity,"Input activity attribute"),label_splitting_target_key:required(form.target,"Output activity attribute"),label_splitting_prefix_length:integer(form.prefix,"Prefix context",0,4096),label_splitting_suffix_length:integer(form.suffix,"Suffix context",0,4096),label_splitting_min_edge_weight:finite(form.weight,"Minimum edge weight"),label_splitting_activities_suffix:required(form.separator,"Label separator")};
  const activities=exactList(form.activities,"Target activities");if(activities!==undefined){if(!activities.length||activities.some(value=>!value)||new Set(activities).size!==activities.length)throw new Error("Target activities must be non-empty unique names.");options.label_splitting_target_activities=activities;}return options;
}
export interface TransformationCase {index:number;case_id:string;event_count:number;}
export interface LabelRow {case_index:number;event_index:number;input:string;previous:string;output:string;}
export interface TransformationRun {family:"LOG_TRANSFORMATION";source_id:string;cases:TransformationCase[];result:LogToFeaturesResult|LogToTargetResult|LabelSplittingResult;transformed_xes:string|null;activity_attribute?:string;complete_activity_field?:boolean;label_rows?:LabelRow[];data_json:string;report_json:string;}
export function isTransformationRun(value:unknown):value is Omit<TransformationRun,"data_json"|"report_json"> {
  if(!value||typeof value!=="object")return false;const run=value as Partial<TransformationRun>;
  if(run.family!=="LOG_TRANSFORMATION"||typeof run.source_id!=="string"||!run.source_id||!Array.isArray(run.cases)||!run.cases.every((trace,index)=>!!trace&&trace.index===index&&typeof trace.case_id==="string"&&Number.isSafeInteger(trace.event_count)&&trace.event_count>=0))return false;
  const cases=run.cases;
  if(isLogToFeaturesResult(run.result)) {
    const r=run.result;if(run.transformed_xes!==null)return false;
    if(r.variant==="event_based"){const longest=cases.reduce((length,trace)=>Math.max(length,trace.event_count),0);return r.event_rows!.length===cases.length&&r.event_rows!.every(rows=>rows.length===longest&&rows.every(row=>row.length===r.feature_names.length));}
    if(r.variant==="trace_based")return r.trace_rows!.length===cases.length&&(!r.case_ids||r.case_ids.length===cases.length);
    return true;
  }
  if(isLogToTargetResult(run.result)) {
    const r=run.result;return run.transformed_xes===null&&r.trace_count===cases.length&&r.rows.length===cases.length&&r.classes.length===r.target_width&&r.rows.every((rows,i)=>(r.padded?rows.length>=cases[i].event_count&&rows.length===(r.rows[0]?.length??0):rows.length===cases[i].event_count)&&rows.every(row=>row.length===r.target_width));
  }
  if(!isLabelSplittingResult(run.result)||typeof run.transformed_xes!=="string"||typeof run.activity_attribute!=="string"||!run.activity_attribute||typeof run.complete_activity_field!=="boolean"||!Array.isArray(run.label_rows)||run.result.log.traces.length!==cases.length||!run.result.log.traces.every((trace,i)=>trace.case_id===cases[i].case_id&&trace.events.length===cases[i].event_count))return false;
  let position=0;for(const trace of cases)for(let event=0;event<trace.event_count;event++){const row=run.label_rows[position++];if(!row||row.case_index!==trace.index||row.event_index!==event||![row.input,row.previous,row.output].every(value=>typeof value==="string"))return false;}return position===run.label_rows.length&&run.complete_activity_field===run.label_rows.every(row=>!!row.output);
}
export async function requestTransformation(method:TransformationMethod,sourceId:string,options:LogToFeaturesOptions|LogToTargetOptions|LabelSplittingOptions,signal?:AbortSignal):Promise<TransformationRun> {
  if(!sourceId)throw new Error("Refresh the transformation source first.");
  const query=method==="features"?logToFeaturesQuery(options as LogToFeaturesOptions):method==="targets"?logToTargetQuery(options as LogToTargetOptions):labelSplittingQuery(options as LabelSplittingOptions);
  const route=method==="features"?"log-to-features":method==="targets"?"transform-target":"label-splitting";const params=new URLSearchParams(query);params.set("source_id",sourceId);params.set("include_source","true");
  const response=await fetch(`/api/${route}?${params}`,{method:"POST",...(signal?{signal}:{})});const raw=await response.text();let value:unknown;try{value=JSON.parse(raw);}catch{throw new Error("The backend returned an invalid transformation response.");}
  if(value&&typeof value==="object"&&"error"in value)throw new Error(String((value as {error:unknown}).error));if(!response.ok)throw new Error(`Transformation returned ${response.status}.`);if(!isTransformationRun(value))throw new Error("The backend returned an invalid transformation result.");if(value.source_id!==sourceId)throw new Error("The source log changed; refresh the transformation source.");
  if(value.result.family!==({features:"LOG_TO_FEATURES",targets:"LOG_TO_TARGET",labels:"LABEL_SPLITTING"} as const)[method])throw new Error("The backend returned a different transformation method.");
  const variant=method==="features"?(options as LogToFeaturesOptions).features_variant:method==="targets"?(options as LogToTargetOptions).target_variant:(options as LabelSplittingOptions).label_splitting_variant;
  if(variant!==undefined&&value.result.variant!==variant)throw new Error("The backend returned a different transformation variant.");
  return {...value,data_json:raw,report_json:`{"family":"TRANSFORMATION_REPORT","request":${JSON.stringify({method,source_id:sourceId,options})},"response":${raw}}`};
}
export function cellText(value:unknown):string {if(value===null)return "null";if(typeof value==="object"&&value!==null&&"value"in value)return String((value as {value:unknown}).value);return String(value);}
export interface MatrixRow {metadata:unknown[];values:unknown[];}
export interface TransformationMatrix {metadataNames:string[];columns:string[];count:number;rowAt:(index:number)=>MatrixRow;}
export function transformationMatrix(run:TransformationRun):TransformationMatrix {
  const r=run.result;
  if(r.family==="LABEL_SPLITTING")return {metadataNames:["source_case_position","source_case_id","event_position"],columns:["Input label","Previous output field","Output field"],count:run.label_rows!.length,rowAt:index=>{const row=run.label_rows![index];return {metadata:[row.case_index,run.cases[row.case_index].case_id,row.event_index],values:[row.input,row.previous,row.output]};}};
  if(r.family==="LOG_TO_FEATURES"&&r.variant==="temporal")return {metadataNames:["bucket_position","timestamp_epoch_seconds","timestamp_epoch_nanoseconds"],columns:r.feature_names,count:r.temporal_rows!.length,rowAt:index=>{const row=r.temporal_rows![index];return {metadata:[index,row.timestamp,row.timestamp_ns??""],values:row.values};}};
  if(r.family==="LOG_TO_FEATURES"&&r.variant==="trace_based")return {metadataNames:["source_case_position","source_case_id",...(r.case_ids?["result_case_id"]:[])],columns:r.feature_names,count:r.trace_rows!.length,rowAt:index=>({metadata:[index,run.cases[index].case_id,...(r.case_ids?[r.case_ids[index]]:[])],values:r.trace_rows![index]})};
  const traces=r.family==="LOG_TO_TARGET"?r.rows:(r as LogToFeaturesResult).event_rows!;const ends:number[]=[];let count=0;for(const trace of traces){count+=trace.length;ends.push(count);}const target=r.family==="LOG_TO_TARGET";
  return {metadataNames:["source_case_position","source_case_id","event_position","row_kind"],columns:target?r.classes:(r as LogToFeaturesResult).feature_names,count,rowAt:index=>{let left=0,right=ends.length;while(left<right){const middle=(left+right)>>>1;if(ends[middle]<=index)left=middle+1;else right=middle;}const event=index-(left?ends[left-1]:0);return {metadata:[left,run.cases[left].case_id,event,event<run.cases[left].event_count?"event":"padding"],values:traces[left][event]};}};
}
export function transformationCsv(run:TransformationRun):Blob {
  const matrix=transformationMatrix(run);const quote=(value:unknown)=>'"'+cellText(value).replaceAll('"','""')+'"';const lines=[[...matrix.metadataNames,...matrix.columns].map(quote).join(",")];
  for(let index=0;index<matrix.count;index++){const row=matrix.rowAt(index);lines.push([...row.metadata,...row.values].map(quote).join(","));}return new Blob([lines.join("\r\n")+"\r\n"],{type:"text/csv;charset=utf-8"});
}
export function transformedXes(run:TransformationRun):Blob {if(run.transformed_xes===null)throw new Error("This transformation does not produce an XES log.");return new Blob([run.transformed_xes],{type:"application/xml"});}
