import {nativeCell} from "./native-json";
import { backendFetch } from "./backend-fetch";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes,type ReferenceLogFilterNativeString} from "./reference-log-filters";

export const PANDAS_VARIANTS_ORIGINAL_OPERATIONS=["membership","top_k","min_coverage","max_coverage"] as const;
export type PandasVariantsOriginalOperation=typeof PANDAS_VARIANTS_ORIGINAL_OPERATIONS[number];
export const PANDAS_VARIANTS_ORIGINAL_LABELS:Record<PandasVariantsOriginalOperation,string>={membership:"Match complete activity sequences",top_k:"Keep the top k variants",min_coverage:"Keep variants above a coverage threshold",max_coverage:"Keep variants below a coverage threshold"};
export interface PandasVariantsOriginalForm {operation:PandasVariantsOriginalOperation;callJson:string;}
export type PandasVariantsOriginalExample="operation"|"mapping"|"dictionary"|"enum"|"raw";
export function pandasVariantsOriginalExample(operation:PandasVariantsOriginalOperation,example:PandasVariantsOriginalExample="operation"):PandasVariantsOriginalForm {
  const str=(value:string)=>({type:"str",value}),tuple={type:"tuple",values:[str("A"),str("B")]};
  const criterion=operation==="membership"?{type:"list",values:[tuple]}:operation==="top_k"?{type:"int",value:1}:{type:"float",hex:"0x1.0000000000000p-1"};
  const keyword_entries:unknown[]=example==="mapping"?[{key:"parameters",value:{type:"dict",items:[[str("variants_df"),{type:"pandas.DataFrame",file:"variants_file"}]]}}]:example==="raw"?[{key:"parameters",value:str("invalid parameters")}]:example==="enum"?[{key:"parameters",value:{type:"dict",items:[[str("pm4py:param:activity_key"),str("missing")],[{type:"enum",module:"pm4py.algo.filtering.pandas.variants.variants_filter",name:"ACTIVITY_KEY",value:str("pm4py:param:activity_key")},str("concept:name")]]}}]:[];
  if(example==="dictionary")return {operation:"membership",callJson:'{"schema":"minera-pandas-variants-original-call-v1","args":[{"type":"dict","items":[[{"type":"tuple","values":[{"type":"str","value":"A"},{"type":"str","value":"B"}]},{"type":"int","value":9007199254740993}]]}],"keyword_entries":[]}'};
  return {operation,callJson:JSON.stringify({schema:"minera-pandas-variants-original-call-v1",args:[criterion],keyword_entries},null,2)};
}
export function defaultPandasVariantsOriginalForm():PandasVariantsOriginalForm{return pandasVariantsOriginalExample("membership");}
export async function pandasVariantsOriginalCallText(file:File):Promise<string>{
  if(!(file instanceof File))throw new Error("Choose an actual original-call JSON File.");
  try{return new TextDecoder("utf-8",{fatal:true,ignoreBOM:true}).decode(await file.arrayBuffer());}
  catch{throw new Error("The original-call file must contain valid UTF-8. Its bytes were not replaced.");}
}
export function pandasVariantsOriginalFields(form:PandasVariantsOriginalForm):Array<[string,string]>{
  if(!(PANDAS_VARIANTS_ORIGINAL_OPERATIONS as readonly unknown[]).includes(form.operation))throw new Error("Choose an available original pandas variants operation.");
  return [["engine","pandas"],["operation",form.operation],["call_json",form.callJson]];
}
export interface PandasVariantsOriginalCounts {rows:number;columns:number;}
export interface PandasVariantsOriginalEnvelope {
  family:"TABLE_VARIANTS_FILTER";engine:"pandas"|null;operation:PandasVariantsOriginalOperation|null;http_status:200|400;
  phase:"control"|"source_import"|"variants_import"|"apply"|"result_export"|"complete";apply_returned:boolean;
  source:PandasVariantsOriginalCounts|null;result:PandasVariantsOriginalCounts|null;source_unchanged:boolean|null;
  case_id_key:null;attribute_key:null;timestamp_key:null;
  source_table_json:ReferenceLogFilterNativeString|null;filtered_table_json:ReferenceLogFilterNativeString|null;variants_table_json:ReferenceLogFilterNativeString|null;variants_source_unchanged:boolean|null;
  error_type:ReferenceLogFilterNativeString|null;error:ReferenceLogFilterNativeString|null;
  call_mode:"original"|null;call_json:ReferenceLogFilterNativeString|null;controls_before_json:ReferenceLogFilterNativeString|null;controls_after_json:ReferenceLogFilterNativeString|null;controls_unchanged:boolean|null;source_after_table_json:ReferenceLogFilterNativeString|null;
}
export const PANDAS_VARIANTS_ORIGINAL_ARTIFACTS=["call_json","source_table_json","source_after_table_json","filtered_table_json","variants_table_json","controls_before_json","controls_after_json"] as const;
export type PandasVariantsOriginalArtifact=typeof PANDAS_VARIANTS_ORIGINAL_ARTIFACTS[number];
export const PANDAS_VARIANTS_ORIGINAL_KEYS=["family","engine","operation","http_status","phase","apply_returned","source","result","source_unchanged","case_id_key","attribute_key","timestamp_key","source_table_json","filtered_table_json","variants_table_json","variants_source_unchanged","error_type","error","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_after_table_json"] as const;
const object=(v:unknown):v is Record<string,unknown>=>v!==null&&typeof v==="object"&&!Array.isArray(v);
const nullableString=(v:unknown)=>v===null||isOriginalEmbeddingFilterNativeString(v);
function counts(v:unknown):boolean{return v===null||object(v)&&Object.keys(v).length===2&&["rows","columns"].every(key=>{
  if(!Object.hasOwn(v,key)||typeof v[key]!=="number"||!Number.isFinite(v[key]))return false;
  const token=String(nativeCell(v,key));return /^(?:0|[1-9]\d*)$/.test(token)&&BigInt(token)<=18446744073709551615n;
});}
export function isPandasVariantsOriginalEnvelope(v:unknown):v is PandasVariantsOriginalEnvelope {
  if(!object(v)||Object.keys(v).length!==24||!PANDAS_VARIANTS_ORIGINAL_KEYS.every(key=>Object.hasOwn(v,key))||v.family!=="TABLE_VARIANTS_FILTER"||
    !(v.engine===null||v.engine==="pandas")||!(v.operation===null||(PANDAS_VARIANTS_ORIGINAL_OPERATIONS as readonly unknown[]).includes(v.operation))||
    !(v.http_status===200||v.http_status===400)||typeof v.phase!=="string"||!["control","source_import","variants_import","apply","result_export","complete"].includes(v.phase)||
    typeof v.apply_returned!=="boolean"||!counts(v.source)||!counts(v.result)||
    ![v.source_unchanged,v.variants_source_unchanged,v.controls_unchanged].every(value=>value===null||typeof value==="boolean")||
    v.case_id_key!==null||v.attribute_key!==null||v.timestamp_key!==null||!(v.call_mode===null||v.call_mode==="original")||
    ![...PANDAS_VARIANTS_ORIGINAL_ARTIFACTS,"error_type","error"].every(key=>nullableString(v[key]))||
    !(v.call_mode===null?v.call_json===null:v.call_json!==null))return false;
  if(v.http_status===400)return v.error!==null&&v.error_type!==null;
  return v.phase==="complete"&&v.engine==="pandas"&&v.operation!==null&&v.call_mode==="original"&&v.apply_returned===true&&
    v.error===null&&v.error_type===null&&v.source!==null&&v.result!==null&&v.source_table_json!==null&&v.source_after_table_json!==null&&
    v.filtered_table_json!==null&&v.controls_before_json!==null&&v.controls_after_json!==null&&v.source_unchanged!==null&&v.controls_unchanged!==null;
}
export interface PandasVariantsOriginalRun {value:PandasVariantsOriginalEnvelope;sourceFile:File;variantsFile:File|null;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export async function requestPandasVariantsOriginal(sourceFile:File,form:PandasVariantsOriginalForm,variantsFile:File|null=null,signal?:AbortSignal):Promise<PandasVariantsOriginalRun>{
  signal?.throwIfAborted();if(!(sourceFile instanceof File))throw new Error("Choose an actual native EventTable source File.");
  if(variantsFile!==null&&!(variantsFile instanceof File))throw new Error("The supplied variants table must be an actual File.");
  const fields=pandasVariantsOriginalFields(form),body=new FormData();body.append("source_file",sourceFile);if(variantsFile)body.append("variants_file",variantsFile);
  for(const[key,value]of fields)appendNativeMultipartField(body,key,value);
  const response=await backendFetch("/api/reference-table-variants",{method:"POST",body,signal});signal?.throwIfAborted();
  if(response.status!==200&&response.status!==400)throw new Error(`Original pandas variants returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());
  try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The original pandas variants response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-table-variants",fields,reference:{name:sourceFile.name,size:sourceFile.size,last_modified:sourceFile.lastModified}});signal?.throwIfAborted();
  if(!isPandasVariantsOriginalEnvelope(value)||value.http_status!==response.status)throw new Error("The original pandas variants response has an invalid native envelope.");
  const early=value.http_status===400&&value.phase==="control";
  if(value.engine!=="pandas"&&!(early&&value.engine===null)||value.operation!==form.operation&&!(early&&value.operation===null))throw new Error("The original pandas variants response belongs to another engine or operation.");
  if(value.call_mode!=="original"&&!(early&&value.call_mode===null))throw new Error("The original pandas variants response belongs to another call mode.");
  if(value.call_json===null){if(!(early&&value.call_mode===null))throw new Error("The original pandas variants response lost the original call.");}
  else{const actual=referenceLogFilterStringBytes(value.call_json),expected=nativeStringBytes(form.callJson);if(actual.length!==expected.length||actual.some((b,i)=>b!==expected[i]))throw new Error("The original pandas variants response belongs to another call.");}
  return {value,sourceFile,variantsFile,fields,raw:nativeResponseJson(value),responseBytes};
}
export function pandasVariantsOriginalArtifactFile(run:PandasVariantsOriginalRun,key:PandasVariantsOriginalArtifact):File {
  const value=run.value[key];if(value===null||key==="filtered_table_json"&&(run.value.http_status!==200||run.value.error!==null))throw new Error("The requested native variants artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`pandas-variants-${key}.json`,{type:"application/json"});
}
export function pandasVariantsOriginalResponseFile(run:PandasVariantsOriginalRun):File{return new File([run.responseBytes],"pandas-original-variants-response.json",{type:"application/json"});}
export interface PandasVariantsOriginalIdentity {source:File|null;variants:File|null;form:PandasVariantsOriginalForm;}
export class PandasVariantsOriginalRequestGate {
  private generation=0;
  issue(v:PandasVariantsOriginalIdentity){return {generation:++this.generation,source:v.source,variants:v.variants,form:JSON.stringify(v.form)};}
  invalidate(){++this.generation;}
  accepts(ticket:ReturnType<PandasVariantsOriginalRequestGate["issue"]>,v:PandasVariantsOriginalIdentity,signal?:AbortSignal){return !signal?.aborted&&ticket.generation===this.generation&&ticket.source===v.source&&ticket.variants===v.variants&&ticket.form===JSON.stringify(v.form);}
}
