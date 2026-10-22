import {nativeCell} from "./native-json";
import {appendNativeMultipartField} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {openReferenceLogFilter,referenceLogFilterStringBytes,type ReferenceLogFilterNativeString,type ReferenceLogFilterOpenLog,type ReferenceLogFilterRun} from "./reference-log-filters";

export type LtlFilterEngine="log"|"pandas"|"polars";
export type LtlPolarsInputKind="LazyFrame"|"DataFrame";
export const LTL_FILTER_OPERATIONS=["eventually","next","four_eyes","different_persons","timestamp_ge","timestamp_le"]as const;
export type LtlFilterOperation=typeof LTL_FILTER_OPERATIONS[number];
export function ltlFilterOperations(engine:LtlFilterEngine){return LTL_FILTER_OPERATIONS.filter(operation=>engine==="log"||!operation.startsWith("timestamp_"));}
export const isLtlHelper=(operation:LtlFilterOperation)=>operation.startsWith("timestamp_");
export const ltlUsesCollection=(engine:LtlFilterEngine,operation:LtlFilterOperation)=>operation==="eventually"||engine==="polars"&&(operation==="next"||operation==="four_eyes");
export const ltlUsesTimestampOrder=(engine:LtlFilterEngine,operation:LtlFilterOperation)=>engine==="polars"&&(operation==="eventually"||operation==="next");
export interface LtlFilterForm {
  engine:LtlFilterEngine;operation:LtlFilterOperation;sourceFormat:"auto"|"csv"|"xes"|"native_log";polarsInputKind:LtlPolarsInputKind;
  valuesKind:"strings"|"typed";valuesJson:string;typedValuesJson:string;aJson:string;bJson:string;cJson:string;targetJson:string;
  positive:boolean;enableTimestamp:"default"|"true"|"false";boundariesMode:"omitted"|"supplied";boundariesJson:string;
  attributeKeyMode:"default"|"custom";attributeKey:string;caseKeyMode:"default"|"custom";caseKey:string;
  resourceKeyMode:"default"|"custom";resourceKey:string;timestampKeyMode:"default"|"custom";timestampKey:string;
  activityKeyMode:"default"|"custom";activityKey:string;helperAJson:string;helperBJson:string;
  csvOptions:boolean;csv:{case_id_column:string;activity_column:string;timestamp_column:string;resource_column:string;delimiter:string};trimFields:boolean;
}
export function defaultLtlFilterForm(engine:LtlFilterEngine="log",operation:LtlFilterOperation="eventually"):LtlFilterForm {
  return {engine,operation,polarsInputKind:"LazyFrame",sourceFormat:"auto",valuesKind:"strings",valuesJson:operation==="next"?'["A","B","C"]':'["A","B"]',typedValuesJson:'{"type":"list","value":[{"type":"string","value":"A"},{"type":"string","value":"B"}]}',aJson:'{"type":"string","value":"A"}',bJson:'{"type":"string","value":"B"}',cJson:'{"type":"string","value":"C"}',targetJson:'{"type":"string","value":"A"}',positive:true,enableTimestamp:"default",boundariesMode:"omitted",boundariesJson:'{"type":"list","value":[{"type":"tuple","value":[{"type":"decimal","value":"0"},{"type":"decimal","value":"60"}]}]}',attributeKeyMode:"default",attributeKey:"concept:name",caseKeyMode:"default",caseKey:"case:concept:name",resourceKeyMode:"default",resourceKey:"org:resource",timestampKeyMode:"default",timestampKey:"time:timestamp",activityKeyMode:"default",activityKey:"concept:name",helperAJson:'{"type":"list","value":[{"type":"integer","value":"1"},{"type":"integer","value":"4"}]}',helperBJson:'{"type":"list","value":[{"type":"tuple","value":[{"type":"integer","value":"1"},{"type":"integer","value":"2"}]},{"type":"tuple","value":[{"type":"integer","value":"3"},{"type":"integer","value":"4"}]}]}',csvOptions:false,csv:{case_id_column:"case_id",activity_column:"activity",timestamp_column:"timestamp",resource_column:"resource",delimiter:","},trimFields:false};
}
export function ltlFilterFields(form:LtlFilterForm):Array<[string,string]> {
  if(!["log","pandas","polars"].includes(form.engine)||!ltlFilterOperations(form.engine).includes(form.operation))throw new Error("Choose an available relationship filter.");
  const fields:Array<[string,string]>=[["engine",form.engine],["operation",form.operation]];
  if(isLtlHelper(form.operation)){fields.push(["helper_a_json",form.helperAJson],["helper_b_json",form.helperBJson]);return fields;}
  if(form.engine==="polars"){
    if(form.polarsInputKind!=="LazyFrame"&&form.polarsInputKind!=="DataFrame")throw new Error("Choose a Polars DataFrame or LazyFrame input.");
    fields.push(["input_kind",form.polarsInputKind]);
  }
  if(![form.attributeKeyMode,form.caseKeyMode,form.resourceKeyMode,form.timestampKeyMode,form.activityKeyMode].every(mode=>mode==="default"||mode==="custom"))throw new Error("Choose default or literal attribute names.");
  fields.push(["positive",String(form.positive)]);
  if(ltlUsesCollection(form.engine,form.operation)){
    if(!["strings","typed"].includes(form.valuesKind))throw new Error("Choose literal string values or an explicit typed value.");
    fields.push(form.valuesKind==="strings"?["values_json",form.valuesJson]:["typed_values_json",form.typedValuesJson]);
  }else if(form.operation==="different_persons")fields.push(["target_json",form.targetJson]);
  else {fields.push(["a_json",form.aJson],["b_json",form.bJson]);if(form.operation==="next")fields.push(["c_json",form.cJson]);}
  if(form.attributeKeyMode==="custom")fields.push(["attribute_key",form.attributeKey]);
  if(form.engine!=="log"&&form.caseKeyMode==="custom")fields.push(["case_id_key",form.caseKey]);
  if(["four_eyes","different_persons"].includes(form.operation)&&form.resourceKeyMode==="custom")fields.push(["resource_key",form.resourceKey]);
  if(ltlUsesTimestampOrder(form.engine,form.operation)&&form.timestampKeyMode==="custom")fields.push(["timestamp_key",form.timestampKey]);
  if(form.operation==="eventually"&&form.engine!=="polars"){
    if(!["default","true","false"].includes(form.enableTimestamp)||!["omitted","supplied"].includes(form.boundariesMode))throw new Error("Choose timestamp and interval controls.");
    if(form.enableTimestamp!=="default")fields.push(["enable_timestamp",form.enableTimestamp]);
    if((form.enableTimestamp==="true"||form.engine==="pandas"&&form.enableTimestamp==="default"&&form.boundariesMode==="supplied")&&form.timestampKeyMode==="custom")fields.push(["timestamp_key",form.timestampKey]);
    if(form.boundariesMode==="supplied")fields.push(["timestamp_diff_boundaries_json",form.boundariesJson]);
  }
  if(form.engine==="log"){
    if(!["auto","csv","xes","native_log"].includes(form.sourceFormat))throw new Error("Choose a log source format.");
    fields.push(["source_format",form.sourceFormat]);if(form.activityKeyMode==="custom")fields.push(["activity_key",form.activityKey]);
    if(form.csvOptions&&(form.sourceFormat==="auto"||form.sourceFormat==="csv"))fields.push(...Object.entries(form.csv),["trim_fields",String(form.trimFields)]as[string,string]);
  }
  return fields;
}
export interface LtlFilterCounts {traces?:number;events?:number;rows?:number;columns?:number;}
export interface LtlFilterEnvelope {
  family:"REFERENCE_LTL_FILTER";engine:LtlFilterEngine|null;operation:LtlFilterOperation|null;result_kind:"log"|"table"|"helper"|null;
  http_status:200|400;phase:string;apply_returned:boolean;source_format:string|null;input_kind:LtlPolarsInputKind|null;source:LtlFilterCounts|null;result:LtlFilterCounts|null;source_unchanged:boolean|null;
  activity_key:ReferenceLogFilterNativeString|null;case_id_key:ReferenceLogFilterNativeString|null;attribute_key:ReferenceLogFilterNativeString|null;resource_key:ReferenceLogFilterNativeString|null;timestamp_key:ReferenceLogFilterNativeString|null;
  source_json:ReferenceLogFilterNativeString|null;source_after_json:ReferenceLogFilterNativeString|null;result_json:ReferenceLogFilterNativeString|null;filtered_xes:ReferenceLogFilterNativeString|null;xes_error:ReferenceLogFilterNativeString|null;helper_result_json:ReferenceLogFilterNativeString|null;error_type:ReferenceLogFilterNativeString|null;error:ReferenceLogFilterNativeString|null;
}
const object=(value:unknown):value is Record<string,unknown>=>value!==null&&typeof value==="object"&&!Array.isArray(value);
function counts(value:unknown){if(value===null)return true;if(!object(value))return false;const keys=Object.hasOwn(value,"traces")?["traces","events"]:["rows","columns"];return Object.keys(value).length===2&&keys.every(key=>typeof value[key]==="number"&&/^(0|[1-9]\d*)$/.test(String(nativeCell(value,key)))&&BigInt(String(nativeCell(value,key)))<=18446744073709551615n);}
const nativeFields=["activity_key","case_id_key","attribute_key","resource_key","timestamp_key","source_json","source_after_json","result_json","filtered_xes","xes_error","helper_result_json","error_type","error"]as const;
export function isLtlFilterEnvelope(value:unknown):value is LtlFilterEnvelope {
  return object(value)&&Object.keys(value).length===25&&value.family==="REFERENCE_LTL_FILTER"&&(value.engine===null||["log","pandas","polars"].includes(String(value.engine)))&&(value.operation===null||LTL_FILTER_OPERATIONS.includes(value.operation as LtlFilterOperation))&&(value.result_kind===null||["log","table","helper"].includes(String(value.result_kind)))&&(value.http_status===200||value.http_status===400)&&typeof value.phase==="string"&&typeof value.apply_returned==="boolean"&&(value.source_format===null||typeof value.source_format==="string")&&(value.input_kind===null||value.engine==="polars"&&(value.input_kind==="LazyFrame"||value.input_kind==="DataFrame"))&&counts(value.source)&&counts(value.result)&&(value.source_unchanged===null||typeof value.source_unchanged==="boolean")&&nativeFields.every(key=>Object.hasOwn(value,key)&&(value[key]===null||isOriginalEmbeddingFilterNativeString(value[key])));
}
export interface LtlFilterRun {value:LtlFilterEnvelope;sourceFile:File|null;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export async function requestLtlFilter(sourceFile:File|null,form:LtlFilterForm,signal?:AbortSignal):Promise<LtlFilterRun> {
  signal?.throwIfAborted();if(!isLtlHelper(form.operation)&&!(sourceFile instanceof File))throw new Error("Choose an actual source File.");
  const fields=ltlFilterFields(form),body=new FormData();const supplied=isLtlHelper(form.operation)?null:sourceFile;if(supplied)body.append("source_file",supplied);
  for(const[key,value]of fields)appendNativeMultipartField(body,key,value);
  const response=await fetch("/api/reference-ltl-filter",{method:"POST",body,signal});signal?.throwIfAborted();if(response.status!==200&&response.status!==400)throw new Error(`Relationship filtering returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The relationship response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-ltl-filter",fields,reference:supplied?{name:supplied.name,size:supplied.size,last_modified:supplied.lastModified}:undefined});signal?.throwIfAborted();
  if(!isLtlFilterEnvelope(value)||value.http_status!==response.status)throw new Error("The relationship response has an invalid native envelope.");
  if(value.engine!==form.engine&&!(value.http_status===400&&value.engine===null)||value.operation!==form.operation&&!(value.http_status===400&&value.operation===null))throw new Error("The relationship response belongs to another engine or operation.");
  const expectedKind=form.engine==="polars"?form.polarsInputKind:null;
  if(value.input_kind!==expectedKind&&!(value.http_status===400&&value.phase==="control"&&value.input_kind===null))throw new Error("The relationship response belongs to another Polars input type.");
  return {value,sourceFile:supplied,fields,raw:nativeResponseJson(value),responseBytes};
}
export const LTL_FILTER_ARTIFACTS=["source_json","source_after_json","result_json","filtered_xes","helper_result_json"]as const;
export function ltlFilterSourceExample(engine:LtlFilterEngine):File {
  const rows=[["case-1","A","alex"],["case-1","B","bea"],["case-1","C","alex"],["case-2","A","alex"],["case-2","C","alex"],["case-2","B","alex"]];
  if(engine==="log")return new File(["case_id,activity,resource,timestamp\n"+rows.map(([id,activity,resource],i)=>`${id},${activity},${resource},2026-01-01T00:00:0${i}Z\n`).join("")],"relationships-example.csv",{type:"text/csv"});
  const table={columns:["case:concept:name","concept:name","org:resource","time:timestamp"],dtypes:[...Array(3).fill(engine==="polars"?"String":"object"),engine==="polars"?"Datetime(time_unit='us', time_zone='UTC')":"datetime64[ns, UTC]"],attributes:[{key:"example",value:"Activity sequence and performers"}],rows:rows.map((row,i)=>[...row.map(value=>({value})),{kind:"date",value:`2026-01-01T00:00:0${i}Z`,typed_value:`2026-01-01T00:00:0${i}Z`}])};
  return new File([JSON.stringify(table,null,2)],`${engine}-relationships-example.json`,{type:"application/json"});
}
export function ltlFilterCountText(value:LtlFilterCounts,key:keyof LtlFilterCounts):string{return String(nativeCell(value,key));}
export type LtlFilterArtifact=typeof LTL_FILTER_ARTIFACTS[number];
export function ltlFilterArtifactFile(run:LtlFilterRun,key:LtlFilterArtifact):File {
  const value=run.value[key];if(value===null||["result_json","filtered_xes","helper_result_json"].includes(key)&&(run.value.http_status!==200||run.value.error!==null||key==="filtered_xes"&&run.value.xes_error!==null))throw new Error("The requested relationship artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`relationships-${run.value.engine??"data"}-${key}.${key==="filtered_xes"?"xes":"json"}`,{type:key==="filtered_xes"?"application/xml":"application/json"});
}
export function ltlFilterResponseFile(run:LtlFilterRun):File{return new File([run.responseBytes],"relationship-filter-response.json",{type:"application/json"});}
export function openLtlFilteredLog(run:LtlFilterRun,onOpen:ReferenceLogFilterOpenLog){if(run.sourceFile===null||run.value.engine!=="log"||run.value.result_kind!=="log")throw new Error("A filtered event log is required for reopening.");openReferenceLogFilter({...run,sourceFile:run.sourceFile,value:{...run.value,family:"REFERENCE_LOG_FILTER",filtered_log_json:run.value.result_json}}as ReferenceLogFilterRun,onOpen);}
export interface LtlFilterIdentity {source:File|null;form:LtlFilterForm;}
export class LtlFilterRequestGate {private generation=0;issue(value:LtlFilterIdentity){return {generation:++this.generation,source:value.source,form:JSON.stringify(value.form)};}invalidate(){++this.generation;}accepts(ticket:ReturnType<LtlFilterRequestGate["issue"]>,value:LtlFilterIdentity,signal?:AbortSignal){return !signal?.aborted&&ticket.generation===this.generation&&ticket.source===value.source&&ticket.form===JSON.stringify(value.form);}}
