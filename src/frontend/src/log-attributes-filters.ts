import {nativeCell} from "./native-json";
import {appendNativeMultipartField} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {openReferenceLogFilter, referenceLogFilterStringBytes, type ReferenceLogFilterNativeString, type ReferenceLogFilterOpenLog, type ReferenceLogFilterRun, type ReferenceLogFilterSourceFormat} from "./reference-log-filters";

export const LOG_ATTRIBUTES_OPERATIONS = ["traces", "events", "numeric_traces", "numeric_events", "trace_values", "top_activities", "relative", "threshold"] as const;
export type LogAttributesOperation = typeof LOG_ATTRIBUTES_OPERATIONS[number];
export const LOG_ATTRIBUTES_LABELS: Record<LogAttributesOperation, string> = {traces:"Match event values and keep whole cases",events:"Keep matching events",numeric_traces:"Match numeric events and keep whole cases",numeric_events:"Keep events in a numeric range",trace_values:"Match trace attribute values",top_activities:"Keep the most frequent activities",relative:"Filter by relative value occurrence",threshold:"Filter using supplied counts and variant traces"};
export interface LogAttributesForm {
  operation: LogAttributesOperation; sourceFormat: ReferenceLogFilterSourceFormat; positive: boolean;
  valuesMode: "strings" | "typed"; valuesJson: string; typedValuesJson: string; lower: string; upper: string; criterion: string;
  activityKeyMode: "default" | "custom"; activityKey: string; attributeKeyMode: "default" | "custom"; attributeKey: string;
  caseKeyMode: "default" | "custom"; caseKey: string; glueMode: "default" | "custom"; glue: string; once: "default" | "true" | "false";
  stream1: boolean; streamKey1: string; streamValue1: string; stream2: boolean; streamKey2: string; streamValue2: string;
  countsJson: string; variantTracesJson: string; variantCountsJson: string;
  csvOptions: boolean; csv: {case_id_column:string;activity_column:string;timestamp_column:string;delimiter:string}; trimFields: boolean;
}
export function defaultLogAttributesForm(operation: LogAttributesOperation = "traces"): LogAttributesForm {return {operation,sourceFormat:"auto",positive:true,valuesMode:"strings",valuesJson:'["A"]',typedValuesJson:'{"type":"list","value":[{"type":"string","value":"A"}]}',lower:"0",upper:"100",criterion:operation === "relative" ? "0.5" : "2",activityKeyMode:"default",activityKey:"concept:name",attributeKeyMode:"default",attributeKey:"concept:name",caseKeyMode:"default",caseKey:operation === "numeric_traces" ? "concept:name" : "case:concept:name",glueMode:"default",glue:"case:",once:"default",stream1:false,streamKey1:"stream1",streamValue1:'{"type":"string","value":"selected"}',stream2:false,streamKey2:"stream2",streamValue2:'{"type":"integer","value":"1"}',countsJson:'{"type":"list","value":[{"type":"tuple","value":[{"type":"string","value":"A"},{"type":"integer","value":"2"}]}]}',variantTracesJson:'{"type":"list","value":[{"type":"tuple","value":[{"type":"tuple","value":[{"type":"string","value":"A"}]},{"type":"list","value":[{"type":"integer","value":"0"}]}]}]}',variantCountsJson:'{"type":"list","value":[{"type":"tuple","value":[{"type":"tuple","value":[{"type":"string","value":"A"}]},{"type":"integer","value":"1"}]}]}',csvOptions:false,csv:{case_id_column:"case_id",activity_column:"activity",timestamp_column:"timestamp",delimiter:","},trimFields:false};}
export const logAttributesUsesCase = (op: LogAttributesOperation) => ["events","numeric_traces","numeric_events","relative","top_activities"].includes(op);
export function logAttributesFields(f: LogAttributesForm): Array<[string,string]> {
  if (!(LOG_ATTRIBUTES_OPERATIONS as readonly string[]).includes(f.operation) || !["auto","csv","xes","native_log"].includes(f.sourceFormat)) throw new Error("Choose a log attribute filter and source format.");
  if (![f.activityKeyMode,f.attributeKeyMode,f.caseKeyMode,f.glueMode].every(v=>v === "default" || v === "custom") || !["default","true","false"].includes(f.once)) throw new Error("Choose default or literal selectors.");
  const fields: Array<[string,string]> = [["operation",f.operation],["source_format",f.sourceFormat]];
  if(f.activityKeyMode === "custom")fields.push(["activity_key",f.activityKey]);
  if(f.operation !== "top_activities" && f.attributeKeyMode === "custom")fields.push(["attribute_key",f.attributeKey]);
  if(f.operation !== "threshold")fields.push(["positive",String(f.positive)]);
  if(["traces","events","trace_values"].includes(f.operation)) {
    if(!["strings","typed"].includes(f.valuesMode))throw new Error("Choose a values format.");
    fields.push(f.valuesMode === "strings" ? ["values_json",f.valuesJson] : ["typed_values_json",f.typedValuesJson]);
  } else if(f.operation === "numeric_traces" || f.operation === "numeric_events")fields.push(["lower",f.lower],["upper",f.upper]);
  else fields.push(["criterion",f.criterion]);
  if(logAttributesUsesCase(f.operation) && f.caseKeyMode === "custom")fields.push(["case_id_key",f.caseKey]);
  if(f.operation === "numeric_traces") {
    if(f.glueMode === "custom")fields.push(["case_id_glue",f.glue]);
    if(f.stream1)fields.push(["stream_filter_key1",f.streamKey1],["stream_filter_value1_json",f.streamValue1]);
    if(f.stream2)fields.push(["stream_filter_key2",f.streamKey2],["stream_filter_value2_json",f.streamValue2]);
  }
  if(f.operation === "relative" && f.once !== "default")fields.push(["keep_once_per_case",f.once]);
  if(f.operation === "threshold")fields.push(["counts_json",f.countsJson],["variant_traces_json",f.variantTracesJson],["variant_counts_json",f.variantCountsJson]);
  if(f.csvOptions && (f.sourceFormat === "auto" || f.sourceFormat === "csv"))fields.push(...Object.entries(f.csv),["trim_fields",String(f.trimFields)] as [string,string]);
  return fields;
}
export interface LogAttributesEnvelope {
  family:"LOG_ATTRIBUTES_FILTER"; operation:LogAttributesOperation|null; http_status:200|400; phase:string; source_format:string|null;
  source:{traces:number;events:number}|null; result:{traces:number;events:number}|null; source_unchanged:boolean|null;
  activity_key:ReferenceLogFilterNativeString|null; filtered_log_json:ReferenceLogFilterNativeString|null; filtered_xes:ReferenceLogFilterNativeString|null; xes_error:ReferenceLogFilterNativeString|null; error:ReferenceLogFilterNativeString|null; filter_attribute_key:ReferenceLogFilterNativeString|null;
  source_log_json:ReferenceLogFilterNativeString|null;source_after_log_json:ReferenceLogFilterNativeString|null;parameters_before_json:ReferenceLogFilterNativeString|null;parameters_after_json:ReferenceLogFilterNativeString|null;parameters_unchanged:boolean|null;
  variants_source_log_json:ReferenceLogFilterNativeString|null;variants_source_after_log_json:ReferenceLogFilterNativeString|null;variants_source_unchanged:boolean|null;threshold_counts_json:ReferenceLogFilterNativeString|null;threshold_variants_map_json:ReferenceLogFilterNativeString|null;variant_counts_json:ReferenceLogFilterNativeString|null;error_type:ReferenceLogFilterNativeString|null;
}
export const LOG_ATTRIBUTES_ARTIFACTS = ["source_log_json","source_after_log_json","parameters_before_json","parameters_after_json","variants_source_log_json","variants_source_after_log_json","threshold_counts_json","threshold_variants_map_json","variant_counts_json","filtered_log_json","filtered_xes"] as const;
export type LogAttributesArtifact = typeof LOG_ATTRIBUTES_ARTIFACTS[number];
const stringFields = ["activity_key","filtered_log_json","filtered_xes","xes_error","error","filter_attribute_key","source_log_json","source_after_log_json","parameters_before_json","parameters_after_json","variants_source_log_json","variants_source_after_log_json","threshold_counts_json","threshold_variants_map_json","variant_counts_json","error_type"] as const;
const object = (v:unknown):v is Record<string,unknown> => v !== null && typeof v === "object" && !Array.isArray(v);
function counts(v:unknown) {return v === null || object(v) && Object.keys(v).length === 2 && ["traces","events"].every(key => typeof v[key] === "number" && /^(0|[1-9]\d*)$/.test(String(nativeCell(v,key))) && BigInt(String(nativeCell(v,key))) <= 18446744073709551615n);}
export function isLogAttributesEnvelope(v:unknown):v is LogAttributesEnvelope {return object(v) && Object.keys(v).length === 26 && v.family === "LOG_ATTRIBUTES_FILTER" && (v.operation === null || LOG_ATTRIBUTES_OPERATIONS.includes(v.operation as LogAttributesOperation)) && (v.http_status === 200 || v.http_status === 400) && typeof v.phase === "string" && (v.source_format === null || typeof v.source_format === "string") && counts(v.source) && counts(v.result) && ["source_unchanged","parameters_unchanged","variants_source_unchanged"].every(k=>v[k] === null || typeof v[k] === "boolean") && stringFields.every(k=>Object.hasOwn(v,k) && (v[k] === null || isOriginalEmbeddingFilterNativeString(v[k])));}
export interface LogAttributesRun {value:LogAttributesEnvelope;sourceFile:File;variantsFile:File|null;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export async function requestLogAttributes(sourceFile:File,form:LogAttributesForm,variantsFile:File|null=null,signal?:AbortSignal):Promise<LogAttributesRun> {
  signal?.throwIfAborted();if(!(sourceFile instanceof File))throw new Error("Choose an actual event log file.");
  if(form.operation === "threshold" && !(variantsFile instanceof File))throw new Error("Choose the actual auxiliary native log file.");
  const fields=logAttributesFields(form),body=new FormData();body.append("source_file",sourceFile);
  const suppliedVariants=form.operation === "threshold" ? variantsFile : null;if(suppliedVariants)body.append("variants_file",suppliedVariants);
  for(const[k,v]of fields)appendNativeMultipartField(body,k,v);
  const response=await fetch("/api/reference-log-attributes",{method:"POST",body,signal});signal?.throwIfAborted();
  if(response.status !== 200 && response.status !== 400)throw new Error(`Log attributes filtering returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The log attributes response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-log-attributes",fields,reference:{name:sourceFile.name,size:sourceFile.size,last_modified:sourceFile.lastModified}});signal?.throwIfAborted();
  if(!isLogAttributesEnvelope(value)||value.http_status !== response.status)throw new Error("The log attributes response has an invalid native envelope.");
  if(value.operation !== form.operation && !(value.http_status === 400 && value.operation === null))throw new Error("The log attributes response belongs to another filter.");
  return {value,sourceFile,variantsFile:suppliedVariants,fields,raw:nativeResponseJson(value),responseBytes};
}
export function logAttributesArtifactFile(run:LogAttributesRun,key:LogAttributesArtifact):File {
  const value=run.value[key];if(value === null || (key === "filtered_log_json" || key === "filtered_xes") && (run.value.http_status !== 200 || run.value.error !== null || key === "filtered_xes" && run.value.xes_error !== null))throw new Error("The requested log attributes artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`attributes-${key === "filtered_xes" ? "filtered.xes" : key+".json"}`,{type:key === "filtered_xes" ? "application/xml" : "application/json"});
}
export function logAttributesResponseFile(run:LogAttributesRun):File{return new File([run.responseBytes],"log-attributes-response.json",{type:"application/json"});}
export function openLogAttributes(run:LogAttributesRun,onOpenLog:ReferenceLogFilterOpenLog) {openReferenceLogFilter({...run,value:{...run.value,family:"REFERENCE_LOG_FILTER"}} as ReferenceLogFilterRun,onOpenLog);}
export interface LogAttributesIdentity {source:File|null;variants:File|null;form:LogAttributesForm;}
export class LogAttributesRequestGate {private generation=0;issue(v:LogAttributesIdentity){return {generation:++this.generation,source:v.source,variants:v.variants,form:JSON.stringify(v.form)};}invalidate(){++this.generation;}accepts(t:ReturnType<LogAttributesRequestGate["issue"]>,v:LogAttributesIdentity,signal?:AbortSignal){return !signal?.aborted && t.generation === this.generation && t.source === v.source && t.variants === v.variants && t.form === JSON.stringify(v.form);}}
export function logAttributesAuxiliaryExample():File {return new File([JSON.stringify({attributes:[{key:"example",value:"Actual cached trace; contents supply exemptions"}],traces:[{case_id:"cached-case",attributes:[{key:"concept:name",value:"cached-case"}],properties:[{key:"cache",value:"retained"}],events:[{activity:"A",attributes:[{key:"concept:name",value:"A"},{key:"activity",value:"A"}]}]}]},null,2)],"attributes-auxiliary-log.json",{type:"application/json"});}
