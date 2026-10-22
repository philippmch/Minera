import {nativeCell,parseNativeJson} from "./native-json";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes,type ReferenceLogFilterNativeString} from "./reference-log-filters";

export const POLARS_VARIANTS_ORIGINAL_OPERATIONS=["membership","from_list","top_k","auto"] as const;
export type PolarsVariantsOriginalOperation=typeof POLARS_VARIANTS_ORIGINAL_OPERATIONS[number];
export const POLARS_VARIANTS_ORIGINAL_LABELS:Record<PolarsVariantsOriginalOperation,string>={membership:"Match complete activity sequences",from_list:"Match serialized variants",top_k:"Keep the top k variants",auto:"Automatically select variants"};
export interface PolarsVariantsOriginalForm {operation:PolarsVariantsOriginalOperation;callJson:string;}
export type PolarsVariantsOriginalExample="operation"|"eager"|"missing"|"none"|"enum"|"raw";
export function polarsVariantsOriginalExample(operation:PolarsVariantsOriginalOperation,example:PolarsVariantsOriginalExample="operation"):PolarsVariantsOriginalForm {
  const str=(value:string)=>({type:"str",value});
  if((example==="missing"||example==="none")&&operation==="auto")operation="membership";
  const key=operation==="membership"?"admitted_variants":operation==="from_list"?"variants":operation==="top_k"?"k":null;
  const criterion=example==="none"?{type:"NoneType",value:null}:operation==="membership"?{type:"list",values:[{type:"list",values:[str("A"),str("B")]}]}:operation==="from_list"?{type:"list",values:[str("A,B")]}:{type:"int",value:1};
  const keyword_entries:Array<{key:string;value:unknown}>=key&&example!=="missing"?[{key,value:criterion}]:[];
  if(example==="raw"||example==="missing")keyword_entries.push({key:"parameters",value:str("malformed parameters")});
  if(example==="enum")keyword_entries.push({key:"parameters",value:{type:"dict",items:[[str("pm4py:param:activity_key"),str("missing")],[{type:"enum",module:"pm4py.algo.filtering.polars.variants.variants_filter",name:"ACTIVITY_KEY",value:str("pm4py:param:activity_key")},str("concept:name")]]}});
  return {operation,callJson:JSON.stringify({schema:"minera-polars-variants-original-call-v1",input_kind:example==="eager"?"DataFrame":"LazyFrame",keyword_entries},null,2)};
}
export function defaultPolarsVariantsOriginalForm():PolarsVariantsOriginalForm{return polarsVariantsOriginalExample("membership");}
export async function polarsVariantsOriginalCallText(file:File):Promise<string>{
  if(!(file instanceof File))throw new Error("Choose an actual original-call JSON File.");
  try{return new TextDecoder("utf-8",{fatal:true,ignoreBOM:true}).decode(await file.arrayBuffer());}
  catch{throw new Error("The original-call file must contain valid UTF-8. Its bytes were not replaced.");}
}
export function polarsVariantsOriginalFields(form:PolarsVariantsOriginalForm):Array<[string,string]>{
  if(!(POLARS_VARIANTS_ORIGINAL_OPERATIONS as readonly unknown[]).includes(form.operation))throw new Error("Choose an available original Polars variants operation.");
  return [["engine","polars"],["operation",form.operation],["call_json",form.callJson]];
}
export interface PolarsVariantsOriginalCounts {rows:number;columns:number;}
export interface PolarsVariantsReturnedMetadata {kind:"EventTable"|"LazyEventTable";row_count_known:boolean;row_count:number|null;table_context_json:ReferenceLogFilterNativeString;}
export interface PolarsVariantsOriginalEnvelope {
  family:"TABLE_VARIANTS_FILTER";engine:"polars"|null;operation:PolarsVariantsOriginalOperation|null;http_status:200|400;
  phase:"control"|"source_import"|"apply"|"collect"|"result_export"|"complete";apply_returned:boolean;
  source:PolarsVariantsOriginalCounts|null;result:PolarsVariantsOriginalCounts|null;source_unchanged:boolean|null;
  case_id_key:null;attribute_key:null;timestamp_key:null;variants_table_json:null;variants_source_unchanged:null;
  source_table_json:ReferenceLogFilterNativeString|null;source_after_table_json:ReferenceLogFilterNativeString|null;filtered_table_json:ReferenceLogFilterNativeString|null;
  error_type:ReferenceLogFilterNativeString|null;error:ReferenceLogFilterNativeString|null;
  call_mode:"original"|null;call_json:ReferenceLogFilterNativeString|null;controls_before_json:ReferenceLogFilterNativeString|null;controls_after_json:ReferenceLogFilterNativeString|null;controls_unchanged:boolean|null;
  returned_table_metadata_json:ReferenceLogFilterNativeString|null;
}
export const POLARS_VARIANTS_ORIGINAL_ARTIFACTS=["call_json","source_table_json","source_after_table_json","filtered_table_json","controls_before_json","controls_after_json","returned_table_metadata_json"] as const;
export type PolarsVariantsOriginalArtifact=typeof POLARS_VARIANTS_ORIGINAL_ARTIFACTS[number];
export const POLARS_VARIANTS_ORIGINAL_KEYS=["family","engine","operation","http_status","phase","apply_returned","source","result","source_unchanged","case_id_key","attribute_key","timestamp_key","source_table_json","filtered_table_json","variants_table_json","variants_source_unchanged","error_type","error","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_after_table_json","returned_table_metadata_json"] as const;
const object=(v:unknown):v is Record<string,unknown>=>v!==null&&typeof v==="object"&&!Array.isArray(v);
const exact=(v:Record<string,unknown>,keys:readonly string[])=>Object.keys(v).length===keys.length&&keys.every(key=>Object.hasOwn(v,key));
const nullableString=(v:unknown)=>v===null||isOriginalEmbeddingFilterNativeString(v);
const uint64=(holder:object,key:string)=>{const value=(holder as Record<string,unknown>)[key],token=String(nativeCell(holder,key));return typeof value==="number"&&Number.isFinite(value)&&/^(?:0|[1-9]\d*)$/.test(token)&&BigInt(token)<=18446744073709551615n;};
const counts=(v:unknown)=>v===null||object(v)&&exact(v,["rows","columns"])&&uint64(v,"rows")&&uint64(v,"columns");
export function isPolarsVariantsOriginalControls(value:ReferenceLogFilterNativeString):boolean {
  try{
    const v=parseNativeJson(new TextDecoder("utf-8",{fatal:true}).decode(referenceLogFilterStringBytes(value)));
    if(!object(v)||!exact(v,["input_kind","keyword_order","argument","parameter_state","parameters","invoked_arm","parameters_passed","native_options_present"])||
      !(v.input_kind==="DataFrame"||v.input_kind==="LazyFrame")||!Array.isArray(v.keyword_order)||!v.keyword_order.every(key=>typeof key==="string"&&["admitted_variants","variants","k","parameters"].includes(key))||new Set(v.keyword_order).size!==v.keyword_order.length||
      !object(v.argument)||!exact(v.argument,["key","present","value_json"])||!(v.argument.key===null||typeof v.argument.key==="string"&&["admitted_variants","variants","k"].includes(v.argument.key))||typeof v.argument.present!=="boolean"||!nullableString(v.argument.value_json)||
      (v.argument.present?v.argument.key===null||v.argument.value_json===null:v.argument.value_json!==null)||
      typeof v.parameter_state!=="string"||!["omitted","null","raw","entries"].includes(v.parameter_state)||!object(v.parameters)||!exact(v.parameters,["native_arm","raw_json","entries"])||!Array.isArray(v.parameters.entries)||
      !v.parameters.entries.every(entry=>object(entry)&&exact(entry,["key","enum_key","value_json"])&&isOriginalEmbeddingFilterNativeString(entry.key)&&typeof entry.enum_key==="boolean"&&isOriginalEmbeddingFilterNativeString(entry.value_json))||
      !(v.parameters.native_arm==="raw_attribute"?isOriginalEmbeddingFilterNativeString(v.parameters.raw_json)&&v.parameters.entries.length===0&&v.parameter_state!=="entries":v.parameters.native_arm==="ordered_parameter_entries"&&v.parameters.raw_json===null&&v.parameter_state==="entries")||
      typeof v.invoked_arm!=="string"||!["missing_required_default_options","criterion_raw_parameters","criterion_ordered_parameters","auto_raw_parameters","auto_ordered_parameters"].includes(v.invoked_arm)||typeof v.parameters_passed!=="boolean"||v.native_options_present!==false)return false;
    return v.parameters_passed===(v.invoked_arm!=="missing_required_default_options");
  }catch{return false;}
}
export function polarsVariantsReturnedMetadata(value:ReferenceLogFilterNativeString|null):PolarsVariantsReturnedMetadata|null {
  if(value===null)return null;
  try{
    const parsed=parseNativeJson(new TextDecoder("utf-8",{fatal:true}).decode(referenceLogFilterStringBytes(value)));
    if(!object(parsed)||!exact(parsed,["kind","row_count_known","row_count","table_context_json"])||!(parsed.kind==="EventTable"||parsed.kind==="LazyEventTable")||typeof parsed.row_count_known!=="boolean"||!isOriginalEmbeddingFilterNativeString(parsed.table_context_json)||
      !(parsed.row_count_known?uint64(parsed,"row_count"):parsed.row_count===null)||parsed.kind==="EventTable"&&!parsed.row_count_known)return null;
    return parsed as unknown as PolarsVariantsReturnedMetadata;
  }catch{return null;}
}
export function isPolarsVariantsOriginalEnvelope(v:unknown):v is PolarsVariantsOriginalEnvelope {
  if(!object(v)||!exact(v,POLARS_VARIANTS_ORIGINAL_KEYS)||v.family!=="TABLE_VARIANTS_FILTER"||!(v.engine===null||v.engine==="polars")||
    !(v.operation===null||(POLARS_VARIANTS_ORIGINAL_OPERATIONS as readonly unknown[]).includes(v.operation))||!(v.http_status===200||v.http_status===400)||
    typeof v.phase!=="string"||!["control","source_import","apply","collect","result_export","complete"].includes(v.phase)||typeof v.apply_returned!=="boolean"||!counts(v.source)||!counts(v.result)||
    ![v.source_unchanged,v.controls_unchanged].every(value=>value===null||typeof value==="boolean")||
    ![v.case_id_key,v.attribute_key,v.timestamp_key,v.variants_table_json,v.variants_source_unchanged].every(value=>value===null)||!(v.call_mode===null||v.call_mode==="original")||
    ![...POLARS_VARIANTS_ORIGINAL_ARTIFACTS,"error_type","error"].every(key=>nullableString(v[key]))||!(v.call_mode===null?v.call_json===null:v.call_json!==null))return false;
  if(v.returned_table_metadata_json!==null&&(!v.apply_returned||polarsVariantsReturnedMetadata(v.returned_table_metadata_json as ReferenceLogFilterNativeString)===null))return false;
  if(![v.controls_before_json,v.controls_after_json].every(value=>value===null||isPolarsVariantsOriginalControls(value as ReferenceLogFilterNativeString)))return false;
  if(v.http_status===400)return v.error!==null&&v.error_type!==null;
  return v.phase==="complete"&&v.engine==="polars"&&v.operation!==null&&v.call_mode==="original"&&v.apply_returned===true&&v.error===null&&v.error_type===null&&
    v.source!==null&&v.result!==null&&v.source_table_json!==null&&v.source_after_table_json!==null&&v.filtered_table_json!==null&&v.controls_before_json!==null&&v.controls_after_json!==null&&v.source_unchanged!==null&&v.controls_unchanged!==null&&v.returned_table_metadata_json!==null;
}
export interface PolarsVariantsOriginalRun {value:PolarsVariantsOriginalEnvelope;sourceFile:File;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export async function requestPolarsVariantsOriginal(sourceFile:File,form:PolarsVariantsOriginalForm,signal?:AbortSignal):Promise<PolarsVariantsOriginalRun>{
  signal?.throwIfAborted();if(!(sourceFile instanceof File))throw new Error("Choose an actual native EventTable source File.");
  const fields=polarsVariantsOriginalFields(form),body=new FormData();body.append("source_file",sourceFile);for(const[key,value]of fields)appendNativeMultipartField(body,key,value);
  const response=await fetch("/api/reference-table-variants",{method:"POST",body,signal});signal?.throwIfAborted();
  if(response.status!==200&&response.status!==400)throw new Error(`Original Polars variants returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());
  try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The original Polars variants response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-table-variants",fields,reference:{name:sourceFile.name,size:sourceFile.size,last_modified:sourceFile.lastModified}});signal?.throwIfAborted();
  if(!isPolarsVariantsOriginalEnvelope(value)||value.http_status!==response.status)throw new Error("The original Polars variants response has an invalid native envelope.");
  const early=value.http_status===400&&value.phase==="control";
  if(value.engine!=="polars"&&!(early&&value.engine===null)||value.operation!==form.operation&&!(early&&value.operation===null))throw new Error("The original Polars variants response belongs to another engine or operation.");
  if(value.call_mode!=="original"&&!(early&&value.call_mode===null))throw new Error("The original Polars variants response belongs to another call mode.");
  if(value.call_json===null){if(!(early&&value.call_mode===null))throw new Error("The original Polars variants response lost the original call.");}
  else{const actual=referenceLogFilterStringBytes(value.call_json),expected=nativeStringBytes(form.callJson);if(actual.length!==expected.length||actual.some((byte,i)=>byte!==expected[i]))throw new Error("The original Polars variants response belongs to another call.");}
  return {value,sourceFile,fields,raw:nativeResponseJson(value),responseBytes};
}
export function polarsVariantsOriginalArtifactFile(run:PolarsVariantsOriginalRun,key:PolarsVariantsOriginalArtifact):File {
  const value=run.value[key];if(value===null||key==="filtered_table_json"&&(run.value.http_status!==200||run.value.error!==null))throw new Error("The requested native Polars variants artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`polars-variants-${key}.json`,{type:"application/json"});
}
/** A response-only diagnostic header. It is never used as a source/result data File. */
export function polarsVariantsOriginalHeaderContextFile(run:PolarsVariantsOriginalRun):File {
  const metadata=polarsVariantsReturnedMetadata(run.value.returned_table_metadata_json);if(metadata===null)throw new Error("The native returned header context is unavailable.");
  return new File([referenceLogFilterStringBytes(metadata.table_context_json)],"polars-variants-returned-header-context.json",{type:"application/json"});
}
export function polarsVariantsOriginalResponseFile(run:PolarsVariantsOriginalRun):File{return new File([run.responseBytes],"polars-original-variants-response.json",{type:"application/json"});}
export interface PolarsVariantsOriginalIdentity {source:File|null;form:PolarsVariantsOriginalForm;}
export class PolarsVariantsOriginalRequestGate {
  private generation=0;
  issue(value:PolarsVariantsOriginalIdentity){return {generation:++this.generation,source:value.source,form:JSON.stringify(value.form)};}
  invalidate(){++this.generation;}
  accepts(ticket:ReturnType<PolarsVariantsOriginalRequestGate["issue"]>,value:PolarsVariantsOriginalIdentity,signal?:AbortSignal){return !signal?.aborted&&ticket.generation===this.generation&&ticket.source===value.source&&ticket.form===JSON.stringify(value.form);}
}
