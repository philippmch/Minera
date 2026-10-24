import {nativeCell} from "./native-json";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes,type ReferenceLogFilterNativeString} from "./reference-log-filters";

export const POLARS_TIMESTAMP_ORIGINAL_OPERATIONS=["apply_events","filter_traces_contained","filter_traces_intersecting","filter_traces_attribute_in_timeframe","apply","apply_auto_filter"] as const;
export type PolarsTimestampOriginalOperation=typeof POLARS_TIMESTAMP_ORIGINAL_OPERATIONS[number];
export const POLARS_TIMESTAMP_ORIGINAL_LABELS:Record<PolarsTimestampOriginalOperation,string>={apply_events:"Keep events in a time window",filter_traces_contained:"Keep cases contained in a time window",filter_traces_intersecting:"Keep cases strictly intersecting a time window",filter_traces_attribute_in_timeframe:"Keep cases with a matching event in a time window",apply:"Apply timestamp helper",apply_auto_filter:"Apply automatic timestamp helper"};
export type PolarsTimestampSourceKind="LazyFrame"|"DataFrame";
export interface PolarsTimestampOriginalForm {operation:PolarsTimestampOriginalOperation;callJson:string;}
export type PolarsTimestampOriginalExample="operation"|"eager"|"typed"|"mapping"|"enum"|"raw";
const ENUM_MODULE="pm4py.algo.filtering.polars.timestamp.timestamp_filter";
export function polarsTimestampOriginalExample(operation:PolarsTimestampOriginalOperation,example:PolarsTimestampOriginalExample="operation"):PolarsTimestampOriginalForm {
  const str=(value:string)=>({type:"str",value});
  const bound=(time:string)=>example==="typed"?{type:"datetime",iso:`2026-06-01T${time}+00:00`,fold:0,offset_seconds:0}:str(`2026-06-01 ${time}`);
  const operands:Array<[string,unknown]>=operation==="apply"||operation==="apply_auto_filter"?[]:[...(operation==="filter_traces_attribute_in_timeframe"?[["attribute",str("concept:name")],["attribute_value",str("A")]] as Array<[string,unknown]>:[]),["dt1",bound("00:00:01")],["dt2",bound("00:00:03")]];
  const parameters=example==="mapping"?{type:"dict",items:[[str("pm4py:param:timestamp_key"),str("time:timestamp")]]}:
    example==="enum"?{type:"dict",items:[[str("pm4py:param:timestamp_key"),str("missing")],[{type:"enum",module:ENUM_MODULE,name:"TIMESTAMP_KEY",value:str("pm4py:param:timestamp_key")},str("time:timestamp")]]}:
    example==="raw"?str("invalid parameters"):null;
  const keyword_entries=[...operands.map(([key,value])=>({key,value})),...(parameters===null?[]:[{key:"parameters",value:parameters}])];
  return {operation,callJson:JSON.stringify({schema:"minera-polars-timestamp-original-call-v1",source_kind:example==="eager"?"DataFrame":"LazyFrame",keyword_entries},null,2)};
}
export function defaultPolarsTimestampOriginalForm():PolarsTimestampOriginalForm{return polarsTimestampOriginalExample("apply_events");}
export async function polarsTimestampOriginalCallText(file:File):Promise<string>{
  if(!(file instanceof File))throw new Error("Choose an actual original-call JSON File.");
  try{return new TextDecoder("utf-8",{fatal:true,ignoreBOM:true}).decode(await file.arrayBuffer());}
  catch{throw new Error("The original-call file must contain valid UTF-8. Its bytes were not replaced.");}
}
export function polarsTimestampOriginalCallFile(form:PolarsTimestampOriginalForm):File{return new File([nativeStringBytes(form.callJson)],"polars-timestamp-call.json",{type:"application/json"});}
export function polarsTimestampOriginalFields(form:PolarsTimestampOriginalForm):Array<[string,string]>{
  if(!(POLARS_TIMESTAMP_ORIGINAL_OPERATIONS as readonly unknown[]).includes(form.operation))throw new Error("Choose an available original Polars timestamp operation.");
  return [["engine","polars"],["operation",form.operation],["call_json",form.callJson]];
}
export interface PolarsTimestampOriginalCounts {rows:number;columns:number;}
export interface PolarsTimestampOriginalEnvelope {
  family:"TABLE_TIMESTAMP_FILTER";engine:"polars"|null;operation:PolarsTimestampOriginalOperation|null;http_status:200|400;
  phase:"control"|"source_import"|"apply"|"collect"|"result_export"|"complete";apply_returned:boolean;
  source:PolarsTimestampOriginalCounts|null;result:PolarsTimestampOriginalCounts|null;source_unchanged:boolean|null;
  case_id_key:null;attribute_key:null;timestamp_key:null;
  source_table_json:ReferenceLogFilterNativeString|null;filtered_table_json:ReferenceLogFilterNativeString|null;
  error_type:ReferenceLogFilterNativeString|null;error:ReferenceLogFilterNativeString|null;
  call_mode:"original"|null;call_json:ReferenceLogFilterNativeString|null;controls_before_json:ReferenceLogFilterNativeString|null;controls_after_json:ReferenceLogFilterNativeString|null;controls_unchanged:boolean|null;
  source_after_table_json:ReferenceLogFilterNativeString|null;call_projection_json:ReferenceLogFilterNativeString|null;
}
export const POLARS_TIMESTAMP_ORIGINAL_ARTIFACTS=["call_json","call_projection_json","source_table_json","source_after_table_json","filtered_table_json","controls_before_json","controls_after_json"] as const;
export type PolarsTimestampOriginalArtifact=typeof POLARS_TIMESTAMP_ORIGINAL_ARTIFACTS[number];
export const POLARS_TIMESTAMP_ORIGINAL_KEYS=["family","engine","operation","http_status","phase","apply_returned","source","result","source_unchanged","case_id_key","attribute_key","timestamp_key","source_table_json","filtered_table_json","error_type","error","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_after_table_json","call_projection_json"] as const;
const object=(v:unknown):v is Record<string,unknown>=>v!==null&&typeof v==="object"&&!Array.isArray(v);
const nullableString=(v:unknown)=>v===null||isOriginalEmbeddingFilterNativeString(v);
function counts(v:unknown):boolean{return v===null||object(v)&&Object.keys(v).length===2&&["rows","columns"].every(key=>{
  if(!Object.hasOwn(v,key)||typeof v[key]!=="number"||!Number.isFinite(v[key]))return false;
  const token=String(nativeCell(v,key));return /^(?:0|[1-9]\d*)$/.test(token)&&BigInt(token)<=18446744073709551615n;
});}
export function isPolarsTimestampOriginalEnvelope(v:unknown):v is PolarsTimestampOriginalEnvelope {
  if(!object(v)||Object.keys(v).length!==POLARS_TIMESTAMP_ORIGINAL_KEYS.length||!POLARS_TIMESTAMP_ORIGINAL_KEYS.every(key=>Object.hasOwn(v,key))||v.family!=="TABLE_TIMESTAMP_FILTER"||
    !(v.engine===null||v.engine==="polars")||!(v.operation===null||(POLARS_TIMESTAMP_ORIGINAL_OPERATIONS as readonly unknown[]).includes(v.operation))||
    !(v.http_status===200||v.http_status===400)||typeof v.phase!=="string"||!["control","source_import","apply","collect","result_export","complete"].includes(v.phase)||
    typeof v.apply_returned!=="boolean"||!counts(v.source)||!counts(v.result)||
    ![v.source_unchanged,v.controls_unchanged].every(value=>value===null||typeof value==="boolean")||
    v.case_id_key!==null||v.attribute_key!==null||v.timestamp_key!==null||!(v.call_mode===null||v.call_mode==="original")||
    ![...POLARS_TIMESTAMP_ORIGINAL_ARTIFACTS,"error_type","error"].every(key=>nullableString(v[key]))||
    !(v.call_mode===null?v.call_json===null:v.call_json!==null))return false;
  if(v.controls_unchanged!==null&&(v.controls_before_json===null||v.controls_after_json===null)||v.source_unchanged!==null&&(v.source_table_json===null||v.source_after_table_json===null))return false;
  if(v.phase==="collect"&&!v.apply_returned||v.http_status===400&&v.phase!=="collect"&&v.phase!=="result_export"&&v.apply_returned)return false;
  if(v.http_status===400)return v.error!==null&&v.error_type!==null&&v.phase!=="complete"&&v.result===null&&v.filtered_table_json===null;
  return v.phase==="complete"&&v.engine==="polars"&&v.operation!==null&&v.call_mode==="original"&&v.apply_returned===true&&
    v.error===null&&v.error_type===null&&v.source!==null&&v.result!==null&&v.source_table_json!==null&&v.source_after_table_json!==null&&
    v.filtered_table_json!==null&&v.controls_before_json!==null&&v.controls_after_json!==null&&v.source_unchanged!==null&&v.controls_unchanged!==null&&v.call_projection_json!==null;
}
export interface PolarsTimestampOriginalRun {value:PolarsTimestampOriginalEnvelope;sourceFile:File;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export async function requestPolarsTimestampOriginal(sourceFile:File,form:PolarsTimestampOriginalForm,signal?:AbortSignal):Promise<PolarsTimestampOriginalRun>{
  signal?.throwIfAborted();if(!(sourceFile instanceof File))throw new Error("Choose an actual native EventTable source File.");
  const fields=polarsTimestampOriginalFields(form),body=new FormData();body.append("source_file",sourceFile);
  for(const[key,value]of fields)appendNativeMultipartField(body,key,value);
  const response=await fetch("/api/reference-table-timestamp",{method:"POST",body,signal});signal?.throwIfAborted();
  if(response.status!==200&&response.status!==400)throw new Error(`Original Polars timestamp returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());
  try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The original Polars timestamp response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-table-timestamp",fields,reference:{name:sourceFile.name,size:sourceFile.size,last_modified:sourceFile.lastModified}});signal?.throwIfAborted();
  if(!isPolarsTimestampOriginalEnvelope(value)||value.http_status!==response.status)throw new Error("The original Polars timestamp response has an invalid native envelope.");
  const early=value.http_status===400&&value.phase==="control";
  if(value.engine!=="polars"&&!(early&&value.engine===null)||value.operation!==form.operation&&!(early&&value.operation===null))throw new Error("The original Polars timestamp response belongs to another engine or operation.");
  if(value.call_json===null){if(!(early&&value.call_mode===null))throw new Error("The original Polars timestamp response lost the original call.");}
  else{const actual=referenceLogFilterStringBytes(value.call_json),expected=nativeStringBytes(form.callJson);if(actual.length!==expected.length||actual.some((b,i)=>b!==expected[i]))throw new Error("The original Polars timestamp response belongs to another call.");}
  return {value,sourceFile,fields,raw:nativeResponseJson(value),responseBytes};
}
export function polarsTimestampOriginalCanReuse(v:PolarsTimestampOriginalEnvelope):boolean{return v.http_status===200&&v.phase==="complete"&&v.error===null&&v.apply_returned&&v.filtered_table_json!==null;}
export function polarsTimestampOriginalArtifactFile(run:PolarsTimestampOriginalRun,key:PolarsTimestampOriginalArtifact):File {
  const value=run.value[key];if(value===null||key==="filtered_table_json"&&!polarsTimestampOriginalCanReuse(run.value))throw new Error("The requested native timestamp artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`polars-timestamp-${key}.json`,{type:"application/json"});
}
export function polarsTimestampOriginalResponseFile(run:PolarsTimestampOriginalRun):File{return new File([run.responseBytes],"polars-original-timestamp-response.json",{type:"application/json"});}
export interface PolarsTimestampOriginalIdentity {source:File|null;form:PolarsTimestampOriginalForm;}
export class PolarsTimestampOriginalRequestGate {
  private generation=0;
  issue(v:PolarsTimestampOriginalIdentity){return {generation:++this.generation,source:v.source,operation:v.form.operation,callJson:v.form.callJson};}
  invalidate(){++this.generation;}
  accepts(t:ReturnType<PolarsTimestampOriginalRequestGate["issue"]>,v:PolarsTimestampOriginalIdentity,signal?:AbortSignal){return !signal?.aborted&&t.generation===this.generation&&t.source===v.source&&t.operation===v.form.operation&&t.callJson===v.form.callJson;}
}
