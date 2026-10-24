import {nativeCell,parseNativeJson} from "./native-json";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes,type ReferenceLogFilterNativeString as NativeString} from "./reference-log-filters";

export const LOG_TIMESTAMP_OPERATIONS=["apply","apply_auto_filter","apply_events","filter_on_trace_attribute","filter_traces_attribute_in_timeframe","filter_traces_completing_in_timeframe","filter_traces_contained","filter_traces_intersecting","filter_traces_starting_in_timeframe","has_attribute_in_timeframe","is_contained","is_intersecting","trace_attr_is_contained"] as const;
export type LogTimestampOperation=typeof LOG_TIMESTAMP_OPERATIONS[number];
export const LOG_TIMESTAMP_PREDICATES:readonly LogTimestampOperation[]=["has_attribute_in_timeframe","is_contained","is_intersecting","trace_attr_is_contained"];
export const LOG_TIMESTAMP_LABELS:Record<LogTimestampOperation,string>={apply:"Apply timestamp helper",apply_auto_filter:"Apply automatic timestamp helper",apply_events:"Keep events in a time window",filter_on_trace_attribute:"Filter a trace timestamp attribute",filter_traces_attribute_in_timeframe:"Match an event attribute in a time window",filter_traces_completing_in_timeframe:"Filter by last-event time",filter_traces_contained:"Keep traces contained in a time window",filter_traces_intersecting:"Keep traces intersecting a time window",filter_traces_starting_in_timeframe:"Filter by first-event time",has_attribute_in_timeframe:"Test an event attribute in one trace",is_contained:"Test whether one trace is contained",is_intersecting:"Test whether one trace intersects",trace_attr_is_contained:"Test a timestamp attribute on one trace"};
export interface LogTimestampForm {operation:LogTimestampOperation;callJson:string;}
export function logTimestampExample(operation:LogTimestampOperation):LogTimestampForm {
  const str=(value:string)=>({type:"str",value});
  const predicate=LOG_TIMESTAMP_PREDICATES.includes(operation),bounds=predicate?["2026-06-01T00:00:01Z","2026-06-01T00:00:03Z"].map(iso=>({type:"datetime",iso,fold:0,offset_seconds:0})):[str("2026-06-01 00:00:01"),str("2026-06-01 00:00:03")];
  const args=operation==="apply"||operation==="apply_auto_filter"?[]:operation==="has_attribute_in_timeframe"?[str("concept:name"),str("A"),...bounds,str("time:timestamp")]:operation==="filter_traces_attribute_in_timeframe"?[str("concept:name"),str("A"),...bounds]:predicate?[...bounds,str(operation==="trace_attr_is_contained"?"started":"time:timestamp")]:bounds;
  return {operation,callJson:JSON.stringify({schema:"minera-log-timestamp-original-call-v1",args,keyword_entries:[],source_trace_index:predicate?0:null},null,2)};
}
export const defaultLogTimestampForm=()=>logTimestampExample("apply_events");
export async function logTimestampCallText(file:File):Promise<string>{
  if(!(file instanceof File))throw new Error("Choose an actual timestamp call File.");
  try{return new TextDecoder("utf-8",{fatal:true,ignoreBOM:true}).decode(await file.arrayBuffer());}
  catch{throw new Error("The timestamp call must contain valid UTF-8. Its bytes were not replaced.");}
}
export function logTimestampFields(form:LogTimestampForm):Array<[string,string]>{
  if(!(LOG_TIMESTAMP_OPERATIONS as readonly unknown[]).includes(form.operation))throw new Error("Choose an available timestamp operation.");
  return [["operation",form.operation],["call_json",form.callJson]];
}
export function logTimestampCallFile(form:LogTimestampForm):File{return new File([nativeStringBytes(form.callJson)],"log-timestamp-call.json",{type:"application/json"});}
export interface LogTimestampCounts {traces:number;events:number;}
export interface LogTimestampEnvelope {
  family:"REFERENCE_LOG_TIMESTAMP";operation:LogTimestampOperation|null;http_status:200|400|500;
  phase:"control"|"source_import"|"native_call"|"result_export"|"complete";
  source_format:"native_log"|null;source:LogTimestampCounts|null;result:LogTimestampCounts|null;source_unchanged:boolean|null;
  call_mode:"original"|null;call_json:NativeString|null;controls_before_json:NativeString|null;controls_after_json:NativeString|null;controls_unchanged:boolean|null;
  source_log_json:NativeString|null;source_after_log_json:NativeString|null;filtered_log_json:NativeString|null;filtered_xes:NativeString|null;xes_error:NativeString|null;
  value_json:NativeString|null;error_type:NativeString|null;error:NativeString|null;native_returned:boolean;result_kind:"log"|"bool"|null;call_projection_json:NativeString|null;inspection_error:NativeString|null;
}
export const LOG_TIMESTAMP_KEYS=["family","operation","http_status","phase","source_format","source","result","source_unchanged","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_log_json","source_after_log_json","filtered_log_json","filtered_xes","xes_error","value_json","error_type","error","native_returned","result_kind","call_projection_json","inspection_error"] as const;
export const LOG_TIMESTAMP_ARTIFACTS=["call_json","call_projection_json","source_log_json","source_after_log_json","controls_before_json","controls_after_json","filtered_log_json","filtered_xes","value_json"] as const;
export type LogTimestampArtifact=typeof LOG_TIMESTAMP_ARTIFACTS[number];
const object=(v:unknown):v is Record<string,unknown>=>v!==null&&typeof v==="object"&&!Array.isArray(v);
const exact=(v:Record<string,unknown>,keys:readonly string[])=>Object.keys(v).length===keys.length&&keys.every(k=>Object.hasOwn(v,k));
const nullableString=(v:unknown)=>v===null||isOriginalEmbeddingFilterNativeString(v);
const uint64=(holder:object,key:string)=>{const v=(holder as Record<string,unknown>)[key],raw=String(nativeCell(holder,key));return typeof v==="number"&&Number.isFinite(v)&&/^(?:0|[1-9]\d*)$/.test(raw)&&BigInt(raw)<=18446744073709551615n;};
const counts=(v:unknown)=>v===null||object(v)&&exact(v,["traces","events"])&&uint64(v,"traces")&&uint64(v,"events");
function artifactJson(value:NativeString):unknown{return parseNativeJson(new TextDecoder("utf-8",{fatal:true}).decode(referenceLogFilterStringBytes(value)));}
export function isLogTimestampControls(value:NativeString):boolean {
  try{
    const v=artifactJson(value);if(!object(v)||!exact(v,["schema","source_argument","arguments","options"])||v.schema!=="minera-log-timestamp-native-controls-v1"||!object(v.source_argument)||!exact(v.source_argument,["kind","source_trace_index","log_json","trace_json"])||!Array.isArray(v.arguments))return false;
    const s=v.source_argument;
    if(s.kind==="log"?s.source_trace_index!==null||!isOriginalEmbeddingFilterNativeString(s.log_json)||s.trace_json!==null:s.kind!=="trace"||!uint64(s,"source_trace_index")||s.log_json!==null||!isOriginalEmbeddingFilterNativeString(s.trace_json))return false;
    return v.arguments.every((a,i)=>object(a)&&exact(a,["name","position","value_json"])&&typeof a.name==="string"&&a.position===i&&isOriginalEmbeddingFilterNativeString(a.value_json))&&
      (v.options===null||object(v.options)&&exact(v.options,["timestamp_key_json","enum_timestamp_key_json","positive_json"])&&isOriginalEmbeddingFilterNativeString(v.options.timestamp_key_json)&&nullableString(v.options.enum_timestamp_key_json)&&isOriginalEmbeddingFilterNativeString(v.options.positive_json));
  }catch{return false;}
}
export function logTimestampBoolean(value:NativeString|null):boolean|null {
  if(value===null)return null;
  try{const v=artifactJson(value);if(!object(v))return null;const table=v.native_schema==="event-table-raw-v1"&&v.native_byte_strings==="hex-v1"?v.value:v;
    if(!object(table)||!Array.isArray(table.rows)||table.rows.length!==1||!Array.isArray(table.rows[0])||table.rows[0].length!==1)return null;
    const cell=table.rows[0][0];if(!object(cell))return null;
    if(typeof cell.typed_value==="boolean")return cell.typed_value;
    if(object(cell.typed_value)&&cell.typed_value.index===3&&typeof cell.typed_value.value==="boolean")return cell.typed_value.value;
    return null;
  }catch{return null;}
}
export function isLogTimestampEnvelope(v:unknown):v is LogTimestampEnvelope {
  if(!object(v)||!exact(v,LOG_TIMESTAMP_KEYS)||v.family!=="REFERENCE_LOG_TIMESTAMP"||!(v.operation===null||(LOG_TIMESTAMP_OPERATIONS as readonly unknown[]).includes(v.operation))||![200,400,500].includes(v.http_status as number)||typeof v.phase!=="string"||!["control","source_import","native_call","result_export","complete"].includes(v.phase)||
    !(v.source_format===null||v.source_format==="native_log")||!counts(v.source)||!counts(v.result)||![v.source_unchanged,v.controls_unchanged].every(x=>x===null||typeof x==="boolean")||!(v.call_mode===null||v.call_mode==="original")||typeof v.native_returned!=="boolean"||!(v.result_kind===null||v.result_kind==="log"||v.result_kind==="bool")||
    ![...LOG_TIMESTAMP_ARTIFACTS,"xes_error","error_type","error","inspection_error"].every(k=>nullableString(v[k]))||!(v.call_mode===null?v.call_json===null:v.call_json!==null))return false;
  if(![v.controls_before_json,v.controls_after_json].every(x=>x===null||isLogTimestampControls(x as NativeString)))return false;
  if(v.controls_unchanged!==null&&(v.controls_before_json===null||v.controls_after_json===null)||v.source_unchanged!==null&&(v.source_log_json===null||v.source_after_log_json===null))return false;
  if(v.native_returned?v.result_kind===null:v.result_kind!==null||v.filtered_log_json!==null||v.filtered_xes!==null||v.value_json!==null||v.result!==null)return false;
  if(v.result_kind==="bool"&&(v.result!==null||v.filtered_log_json!==null||v.filtered_xes!==null||v.xes_error!==null)||v.result_kind==="log"&&v.value_json!==null)return false;
  if(v.http_status!==200)return v.error!==null&&v.error_type!==null&&(v.http_status!==400||!v.native_returned)&&v.phase!=="complete";
  if(v.phase!=="complete"||v.call_mode!=="original"||v.operation===null||v.source_format!=="native_log"||v.source===null||!v.native_returned||v.error!==null||v.error_type!==null||v.inspection_error!==null||v.source_log_json===null||v.source_after_log_json===null||v.controls_before_json===null||v.controls_after_json===null||v.source_unchanged===null||v.controls_unchanged===null||v.call_projection_json===null)return false;
  if(v.result_kind==="bool")return LOG_TIMESTAMP_PREDICATES.includes(v.operation as LogTimestampOperation)&&v.value_json!==null&&logTimestampBoolean(v.value_json as NativeString)!==null;
  return v.result_kind==="log"&&!LOG_TIMESTAMP_PREDICATES.includes(v.operation as LogTimestampOperation)&&v.result!==null&&v.filtered_log_json!==null&&(v.filtered_xes!==null&&v.xes_error===null||v.filtered_xes===null&&v.xes_error!==null);
}
export interface LogTimestampRun {value:LogTimestampEnvelope;sourceFile:File;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export async function requestLogTimestamp(sourceFile:File,form:LogTimestampForm,signal?:AbortSignal):Promise<LogTimestampRun>{
  signal?.throwIfAborted();if(!(sourceFile instanceof File))throw new Error("Choose an actual native event log File. Predicates also require the source log.");
  const fields=logTimestampFields(form),body=new FormData();body.append("source_file",sourceFile);for(const [key,value]of fields)appendNativeMultipartField(body,key,value);
  const response=await fetch("/api/reference-log-timestamp",{method:"POST",body,signal});signal?.throwIfAborted();
  if(![200,400,500].includes(response.status))throw new Error(`Timestamp call returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The timestamp response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-log-timestamp",fields,reference:{name:sourceFile.name,size:sourceFile.size,last_modified:sourceFile.lastModified}});signal?.throwIfAborted();
  if(!isLogTimestampEnvelope(value)||value.http_status!==response.status)throw new Error("The timestamp response has an invalid native envelope.");
  const early=value.phase==="control"&&value.http_status===400;
  if(value.operation!==form.operation&&!(early&&value.operation===null))throw new Error("The timestamp response belongs to another operation.");
  if(value.call_json===null){if(!(early&&value.call_mode===null))throw new Error("The timestamp response lost the original call.");}
  else{const actual=referenceLogFilterStringBytes(value.call_json),expected=nativeStringBytes(form.callJson);if(actual.length!==expected.length||actual.some((b,i)=>b!==expected[i]))throw new Error("The timestamp response belongs to another original call.");}
  return {value,sourceFile,fields,raw:nativeResponseJson(value),responseBytes};
}
export function logTimestampCanReuse(v:LogTimestampEnvelope):boolean{return v.http_status===200&&v.phase==="complete"&&v.error===null&&v.inspection_error===null&&v.native_returned&&v.result_kind==="log"&&v.filtered_log_json!==null;}
export function logTimestampArtifactFile(run:LogTimestampRun,key:LogTimestampArtifact):File {
  const value=run.value[key];if(value===null)throw new Error("The requested timestamp artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`log-timestamp-${key}.${key==="filtered_xes"?"xes":"json"}`,{type:key==="filtered_xes"?"application/xml":"application/json"});
}
export function logTimestampResponseFile(run:LogTimestampRun):File{return new File([run.responseBytes],"log-timestamp-response.json",{type:"application/json"});}
export const logTimestampCountText=(count:LogTimestampCounts,key:keyof LogTimestampCounts)=>String(nativeCell(count,key));
export interface LogTimestampIdentity {source:File|null;form:LogTimestampForm;}
export class LogTimestampRequestGate {
  private generation=0;
  issue(v:LogTimestampIdentity){return {generation:++this.generation,source:v.source,operation:v.form.operation,callJson:v.form.callJson};}
  invalidate(){++this.generation;}
  accepts(t:ReturnType<LogTimestampRequestGate["issue"]>,v:LogTimestampIdentity,signal?:AbortSignal){return !signal?.aborted&&t.generation===this.generation&&t.source===v.source&&t.operation===v.form.operation&&t.callJson===v.form.callJson;}
}
export function logTimestampSourceExample():File {
  const date=(key:string,value:string)=>({key,value,kind_index:1,typed_value:{index:4,value:{value,resolution_index:0,timezone:null}}});
  const log={attributes:[{key:"example",value:"Timestamp calls"}],traces:[0,1].map(i=>({case_id:`case-${i+1}`,attributes:[{key:"concept:name",value:`case-${i+1}`},date("started",`2026-06-01T00:00:0${i+1}`)],properties:[{key:"context",value:"Retained trace property"}],events:["A","B"].map((activity,j)=>({activity,attributes:[{key:"concept:name",value:activity},date("time:timestamp",`2026-06-01T00:00:0${i+j+1}`)]}))}))};
  return new File([JSON.stringify(log,null,2)],"log-timestamp-example.json",{type:"application/json"});
}
