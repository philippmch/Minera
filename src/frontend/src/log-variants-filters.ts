import {nativeCell} from "./native-json";
import { backendFetch } from "./backend-fetch";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes,type ReferenceLogFilterNativeString} from "./reference-log-filters";

export const LOG_VARIANTS_OPERATIONS=["apply","filter_log_variants_percentage","filter_variants_by_coverage_percentage","filter_variants_by_maximum_coverage_percentage","filter_variants_top_k","filter_variants_variants_percentage","find_auto_threshold"] as const;
export type LogVariantsOperation=typeof LOG_VARIANTS_OPERATIONS[number];
export const LOG_VARIANTS_LABELS:Record<LogVariantsOperation,string>={apply:"Match complete activity variants",filter_log_variants_percentage:"Keep variants to cumulative coverage",filter_variants_by_coverage_percentage:"Keep variants above a coverage threshold",filter_variants_by_maximum_coverage_percentage:"Keep variants below a coverage threshold",filter_variants_top_k:"Keep the top k variants",filter_variants_variants_percentage:"Filter using supplied variant traces",find_auto_threshold:"Find the automatic threshold"};
export interface LogVariantsForm {operation:LogVariantsOperation;callJson:string;}
export function logVariantsOriginalExample(operation:LogVariantsOperation):string {
  const text=(value:string)=>({type:"str",value});
  const variant=(...values:string[])=>({type:"tuple",values:values.map(text)});
  const traces=(...indices:number[])=>({type:"list",values:indices.map(source_trace_index=>({type:"trace_ref",source_trace_index}))});
  const dictionary={type:"dict",items:[[variant("A","B"),traces(0,2)],[variant("A","C"),traces(1)]]};
  const half={type:"float",hex:"0x1.0000000000000p-1"};
  const args=operation==="apply"?[{type:"list",values:[variant("A","B")]}]:operation==="filter_variants_top_k"?[{type:"int",value:1}]:operation==="filter_variants_variants_percentage"||operation==="find_auto_threshold"?[dictionary,half]:[half];
  return JSON.stringify({schema:"minera-log-variants-original-call-v1",args,keyword_entries:[]},null,2);
}
export function defaultLogVariantsForm(operation:LogVariantsOperation="apply"):LogVariantsForm{return {operation,callJson:logVariantsOriginalExample(operation)};}
export async function logVariantsCallFileText(file:File):Promise<string>{
  if(!(file instanceof File))throw new Error("Choose an actual original-call JSON File.");
  try{return new TextDecoder("utf-8",{fatal:true,ignoreBOM:true}).decode(await file.arrayBuffer());}
  catch{throw new Error("The original-call file must contain valid UTF-8. Its bytes were not replaced.");}
}
export function logVariantsFields(form:LogVariantsForm):Array<[string,string]>{
  if(!(LOG_VARIANTS_OPERATIONS as readonly string[]).includes(form.operation))throw new Error("Choose a log variants operation.");
  return [["operation",form.operation],["call_json",form.callJson]];
}
export interface LogVariantsCounts {traces:number;events:number;}
export interface LogVariantsEnvelope {
  family:"REFERENCE_LOG_VARIANTS";operation:LogVariantsOperation|null;http_status:200|400;phase:"control"|"source_import"|"filter"|"result_export"|"complete";
  source_format:"native_log"|null;source:LogVariantsCounts|null;result:LogVariantsCounts|null;source_unchanged:boolean|null;
  call_mode:"original"|null;call_json:ReferenceLogFilterNativeString|null;
  controls_before_json:ReferenceLogFilterNativeString|null;controls_after_json:ReferenceLogFilterNativeString|null;controls_unchanged:boolean|null;
  source_log_json:ReferenceLogFilterNativeString|null;source_after_log_json:ReferenceLogFilterNativeString|null;
  filtered_log_json:ReferenceLogFilterNativeString|null;filtered_xes:ReferenceLogFilterNativeString|null;xes_error:ReferenceLogFilterNativeString|null;
  value_json:ReferenceLogFilterNativeString|null;error_type:string|null;error:ReferenceLogFilterNativeString|null;
}
export const LOG_VARIANTS_KEYS=["family","operation","http_status","phase","source_format","source","result","source_unchanged","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_log_json","source_after_log_json","filtered_log_json","filtered_xes","xes_error","value_json","error_type","error"] as const;
export const LOG_VARIANTS_ARTIFACTS=["call_json","controls_before_json","controls_after_json","source_log_json","source_after_log_json","filtered_log_json","filtered_xes","value_json"] as const;
export type LogVariantsArtifact=typeof LOG_VARIANTS_ARTIFACTS[number];
const object=(v:unknown):v is Record<string,unknown>=>v!==null&&typeof v==="object"&&!Array.isArray(v);
const nullableString=(v:unknown)=>v===null||isOriginalEmbeddingFilterNativeString(v);
function counts(v:unknown):boolean{return v===null||object(v)&&Object.keys(v).length===2&&["traces","events"].every(key=>{
  if(!Object.hasOwn(v,key)||typeof v[key]!=="number"||!Number.isFinite(v[key]))return false;
  const token=String(nativeCell(v,key));return /^(?:0|[1-9]\d*)$/.test(token)&&BigInt(token)<=18446744073709551615n;
});}
export function isLogVariantsEnvelope(v:unknown):v is LogVariantsEnvelope {
  if(!object(v)||Object.keys(v).length!==21||!LOG_VARIANTS_KEYS.every(k=>Object.hasOwn(v,k))||v.family!=="REFERENCE_LOG_VARIANTS"||
    !(v.operation===null||(LOG_VARIANTS_OPERATIONS as readonly unknown[]).includes(v.operation))||!(v.http_status===200||v.http_status===400)||
    typeof v.phase!=="string"||!["control","source_import","filter","result_export","complete"].includes(v.phase)||
    !(v.source_format===null||v.source_format==="native_log")||!counts(v.source)||!counts(v.result)||
    ![v.source_unchanged,v.controls_unchanged].every(x=>x===null||typeof x==="boolean")||
    !(v.call_mode===null||v.call_mode==="original")||!(v.error_type===null||typeof v.error_type==="string")||
    ![...LOG_VARIANTS_ARTIFACTS,"xes_error","error"].every(k=>nullableString(v[k]))||
    !(v.call_mode===null?v.call_json===null:v.call_json!==null))return false;
  if(v.http_status===400)return v.error!==null&&v.error_type!==null&&v.filtered_log_json===null&&v.filtered_xes===null&&v.value_json===null;
  if(v.phase!=="complete"||v.call_mode!=="original"||v.operation===null||v.source_format!=="native_log"||v.source===null||
    v.error!==null||v.error_type!==null||v.source_log_json===null||v.source_after_log_json===null||
    v.controls_before_json===null||v.controls_after_json===null||v.source_unchanged===null||v.controls_unchanged===null)return false;
  if(v.operation==="find_auto_threshold")return v.value_json!==null&&v.result===null&&v.filtered_log_json===null&&v.filtered_xes===null&&v.xes_error===null;
  return v.result!==null&&v.filtered_log_json!==null&&v.value_json===null&&
    (v.filtered_xes!==null&&v.xes_error===null||v.filtered_xes===null&&v.xes_error!==null);
}
export interface LogVariantsRun {value:LogVariantsEnvelope;sourceFile:File;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export async function requestLogVariants(sourceFile:File,form:LogVariantsForm,signal?:AbortSignal):Promise<LogVariantsRun>{
  signal?.throwIfAborted();if(!(sourceFile instanceof File))throw new Error("Choose an actual native event log JSON File. Every operation requires its source.");
  const fields=logVariantsFields(form),body=new FormData();body.append("source_file",sourceFile);
  for(const[k,v]of fields)appendNativeMultipartField(body,k,v);
  const response=await backendFetch("/api/reference-log-variants",{method:"POST",body,signal});signal?.throwIfAborted();
  if(response.status!==200&&response.status!==400)throw new Error(`Log variants returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());
  try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The log variants response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-log-variants",fields,reference:{name:sourceFile.name,size:sourceFile.size,last_modified:sourceFile.lastModified}});signal?.throwIfAborted();
  if(!isLogVariantsEnvelope(value)||value.http_status!==response.status)throw new Error("The log variants response has an invalid native envelope.");
  const early=value.http_status===400&&value.phase==="control";
  if(value.operation!==form.operation&&!(early&&value.operation===null))throw new Error("The log variants response belongs to another operation.");
  if(value.call_mode!=="original"&&!(early&&value.call_mode===null))throw new Error("The log variants response belongs to another call mode.");
  if(value.call_json===null){if(!(early&&value.call_mode===null))throw new Error("The log variants response lost the original call.");}
  else{const expected=nativeStringBytes(form.callJson),actual=referenceLogFilterStringBytes(value.call_json);if(actual.length!==expected.length||actual.some((b,i)=>b!==expected[i]))throw new Error("The log variants response belongs to another original call.");}
  return {value,sourceFile,fields,raw:nativeResponseJson(value),responseBytes};
}
export function logVariantsArtifactFile(run:LogVariantsRun,key:LogVariantsArtifact):File {
  const v=run.value,value=v[key],success=v.http_status===200&&v.error===null;
  if(key==="value_json"&&v.operation!=="find_auto_threshold"||["filtered_log_json","filtered_xes"].includes(key)&&v.operation==="find_auto_threshold")throw new Error("This operation does not provide that variants artifact.");
  if(value===null||["filtered_log_json","filtered_xes","value_json"].includes(key)&&(!success||key==="filtered_xes"&&v.xes_error!==null))throw new Error("The requested variants artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`log-variants-${key}.${key==="filtered_xes"?"xes":"json"}`,{type:key==="filtered_xes"?"application/xml":"application/json"});
}
export function logVariantsResponseFile(run:LogVariantsRun):File{return new File([run.responseBytes],"log-variants-response.json",{type:"application/json"});}
export function logVariantsCountText(value:LogVariantsCounts,key:keyof LogVariantsCounts):string{return String(nativeCell(value,key));}
export interface LogVariantsIdentity {source:File|null;form:LogVariantsForm;}
export class LogVariantsRequestGate {
  private generation=0;
  issue(v:LogVariantsIdentity){return {generation:++this.generation,source:v.source,form:JSON.stringify(v.form)};}
  invalidate(){++this.generation;}
  accepts(ticket:ReturnType<LogVariantsRequestGate["issue"]>,v:LogVariantsIdentity,signal?:AbortSignal){return !signal?.aborted&&ticket.generation===this.generation&&ticket.source===v.source&&ticket.form===JSON.stringify(v.form);}
}
export function logVariantsSourceExample():File {
  const sequences=[["A","B"],["A","C"],["A","B"]];
  const log={attributes:[{key:"example",value:"Activity variants with complete trace context"}],metadata:{properties:[{key:"example",value:"retained log property"}]},traces:sequences.map((sequence,index)=>({case_id:`case-${index+1}`,attributes:[{key:"concept:name",value:`case-${index+1}`}],properties:[{key:"example",value:"retained trace property"}],events:sequence.map((activity,event)=>({activity,attributes:[{key:"concept:name",value:activity},{key:"payload",value:`event-${event+1}`}]}))}))};
  return new File([JSON.stringify(log,null,2)],"log-variants-example.json",{type:"application/json"});
}
