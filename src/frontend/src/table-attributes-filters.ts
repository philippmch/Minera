import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isTableRepetitionEnvelope, tableRepetitionSourceExample, type TableRepetitionEngine, type TableRepetitionEnvelope} from "./table-attribute-repetition";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes, type ReferenceLogFilterNativeString} from "./reference-log-filters";

export const TABLE_ATTRIBUTES_OPERATIONS = ["traces", "events", "numeric_traces", "numeric_events", "threshold", "top_activities", "on_values", "relative", "counts"] as const;
export type TableAttributesOperation = typeof TABLE_ATTRIBUTES_OPERATIONS[number];
export const TABLE_ATTRIBUTES_LABELS: Record<TableAttributesOperation, string> = {
  traces: "Keep cases containing attribute values", events: "Keep events matching attribute values",
  numeric_traces: "Keep cases containing values in a numeric interval", numeric_events: "Keep events in a numeric interval",
  threshold: "Keep activities above an occurrence threshold", top_activities: "Keep the most frequent activities",
  on_values: "Filter cases with direct value controls", relative: "Keep values above a relative occurrence threshold", counts: "Count attribute values",
};
export function tableAttributesOperations(engine: TableRepetitionEngine): TableAttributesOperation[] {return TABLE_ATTRIBUTES_OPERATIONS.filter(operation => engine === "polars" || operation !== "counts");}
export interface TableAttributesForm {
  callMode:"friendly"|"original";callJson:string;
  engine: TableRepetitionEngine; operation: TableAttributesOperation; valuesKind: "strings" | "typed"; valuesJson: string;
  lower: string; upper: string; maxActivities: string; threshold: string; minRelative: string;
  positive: boolean; keepMissing: boolean; keepOnceMode: "default" | "false" | "true";
  streamOne: boolean; streamOneKey: string; streamOneValueJson: string; streamTwo: boolean; streamTwoKey: string; streamTwoValueJson: string;
  caseKeyMode: "default" | "custom"; caseKey: string; attributeKeyMode: "default" | "custom"; attributeKey: string;
  activityKeyMode: "default" | "custom"; activityKey: string;
  countsMode: "computed" | "supplied" | "value"; countsJson: string; mostCommonMode: "omitted" | "supplied"; mostCommonJson: string;
}
export function defaultTableAttributesForm(engine: TableRepetitionEngine): TableAttributesForm {
  return {engine,operation:"traces",callMode:"friendly",callJson:tableAttributesOriginalExample("traces"),valuesKind:"strings",valuesJson:'["A"]',lower:"9007199254740992",upper:"9007199254740994",maxActivities:"2",threshold:"2",minRelative:"0.5",positive:true,keepMissing:false,keepOnceMode:"default",
    streamOne:false,streamOneKey:"concept:name",streamOneValueJson:'{"type":"string","value":"A"}',streamTwo:false,streamTwoKey:"concept:name",streamTwoValueJson:'{"type":"string","value":"B"}',
    caseKeyMode:"default",caseKey:"case:concept:name",attributeKeyMode:"default",attributeKey:"concept:name",activityKeyMode:"default",activityKey:"concept:name",countsMode:"computed",
    countsJson:'{"type":"list","value":[{"type":"tuple","value":[{"type":"string","value":"A"},{"type":"integer","value":"3"}]}]}',mostCommonMode:"omitted",mostCommonJson:'{"type":"list","value":[{"type":"string","value":"A"}]}'};
}
export function tableAttributesOriginalExample(operation:TableAttributesOperation):string {
  const integer=(value:string)=>({type:"integer",value});
  const args=operation.startsWith("numeric_")?[integer("0"),integer("100")]:operation==="top_activities"?[]:operation==="threshold"?[integer("2")]:operation==="relative"?[{type:"decimal",value:"0.5"}]:[{type:"list",value:[{type:"string",value:"A"}]}];
  return JSON.stringify({schema:"minera-pandas-attributes-original-call-v1",args},null,2);
}
export async function tableAttributesCallFileText(file:File):Promise<string> {
  if(!(file instanceof File))throw new Error("Choose an actual original-call JSON File.");
  try{return new TextDecoder("utf-8",{fatal:true,ignoreBOM:true}).decode(await file.arrayBuffer());}
  catch{throw new Error("The original-call file must contain valid UTF-8. Its bytes were not replaced.");}
}
export interface TableAttributesEnvelope extends Omit<TableRepetitionEnvelope,"family"> {
  call_mode:"friendly"|"original"|null;call_json:ReferenceLogFilterNativeString|null;
  controls_before_json:ReferenceLogFilterNativeString|null;controls_after_json:ReferenceLogFilterNativeString|null;controls_unchanged:boolean|null;
  family:"TABLE_ATTRIBUTES_FILTER";operation:TableAttributesOperation|null;result_kind:"table"|"counts"|null;
  counts_table_json:ReferenceLogFilterNativeString|null;source_after_table_json:ReferenceLogFilterNativeString|null;
}
export interface TableAttributesRun {value:TableAttributesEnvelope;sourceFile:File;fields:Array<[string,string]>;raw:string;responseBytes:Uint8Array<ArrayBuffer>;}
export function isTableAttributesEnvelope(value:unknown):value is TableAttributesEnvelope {
  if(value===null || typeof value!=="object" || Array.isArray(value))return false;
  const record=value as Record<string,unknown>;
  if(record.family!=="TABLE_ATTRIBUTES_FILTER" || Object.keys(record).length!==23 || !(record.operation===null || TABLE_ATTRIBUTES_OPERATIONS.includes(record.operation as TableAttributesOperation)) ||
    !(record.call_mode===null||record.call_mode==="friendly"||record.call_mode==="original") ||
    !((record.call_mode===null||record.call_mode==="friendly")&&record.call_json===null||record.call_mode==="original"&&record.call_json!==null) ||
    !(record.controls_unchanged===null||typeof record.controls_unchanged==="boolean") ||
    !(record.result_kind===null || record.result_kind==="table" || record.result_kind==="counts") ||
    ![record.counts_table_json,record.source_after_table_json,record.call_json,record.controls_before_json,record.controls_after_json].every(item=>item===null || isOriginalEmbeddingFilterNativeString(item)))return false;
  const {family:_family,operation:_operation,result_kind:_kind,counts_table_json:_counts,source_after_table_json:_after,call_mode:_mode,call_json:_call,controls_before_json:_controlsBefore,controls_after_json:_controlsAfter,controls_unchanged:_unchanged,...rest}=record;
  return isTableRepetitionEnvelope({...rest,family:"TABLE_ATTRIBUTE_REPETITION"});
}
export function tableAttributesFields(form:TableAttributesForm):Array<[string,string]> {
  if(!["pandas","polars"].includes(form.engine) || !tableAttributesOperations(form.engine).includes(form.operation))throw new Error("Choose an available attribute operation.");
  if(form.callMode!=="friendly"&&form.callMode!=="original")throw new Error("Choose friendly controls or an original typed call.");
  if(form.callMode==="original"){
    if(form.engine!=="pandas")throw new Error("Original typed attributes calls require pandas.");
    return [["engine",form.engine],["operation",form.operation],["call_json",form.callJson]];
  }
  if(![form.caseKeyMode,form.attributeKeyMode,form.activityKeyMode].every(mode=>mode==="default" || mode==="custom"))throw new Error("Choose default or literal columns.");
  const fields:Array<[string,string]>=[["engine",form.engine],["operation",form.operation]];
  const membership=["traces","events","on_values"].includes(form.operation),numeric=form.operation.startsWith("numeric_"),frequency=form.operation==="threshold" || form.operation==="top_activities";
  if(membership) {
    if(!["strings","typed"].includes(form.valuesKind))throw new Error("Choose literal string values or an explicit typed collection.");
    fields.push([form.valuesKind==="strings"?"values_json":"typed_values_json",form.valuesJson]);
  }
  if(numeric)fields.push(["lower",form.lower],["upper",form.upper]);
  if(membership || numeric || form.operation==="relative")fields.push(["positive",String(form.positive)]);
  if(form.operation==="threshold")fields.push(["criterion",form.threshold]);
  if(form.operation==="top_activities")fields.push(["criterion",form.maxActivities]);
  if(form.operation==="relative")fields.push(["criterion",form.minRelative]);
  if(form.operation==="relative" || form.operation==="counts") {
    if(!["default","false","true"].includes(form.keepOnceMode))throw new Error("Choose an occurrence-counting mode.");
    if(form.keepOnceMode!=="default")fields.push(["keep_once_per_case",form.keepOnceMode]);
  }
  if(form.engine==="pandas" && !frequency)fields.push(["keep_nan_values",String(form.keepMissing)]);
  if(form.caseKeyMode==="custom" && (["traces","numeric_traces","on_values"].includes(form.operation) || form.operation==="relative" && form.keepOnceMode!=="false" || form.operation==="counts" && form.keepOnceMode==="true"))fields.push(["case_id_key",form.caseKey]);
  if(form.operation==="numeric_traces")for(const [enabled,key,value,name]of [[form.streamOne,form.streamOneKey,form.streamOneValueJson,"1"],[form.streamTwo,form.streamTwoKey,form.streamTwoValueJson,"2"]]as const)
    if(enabled)fields.push([`stream_filter_key${name}`,key],[`stream_filter_value${name}_json`,value]);
  if(frequency) {if(form.activityKeyMode==="custom")fields.push(["activity_key",form.activityKey]);}
  else if(form.attributeKeyMode==="custom")fields.push(["attribute_key",form.attributeKey]);
  if(form.operation==="threshold") {
    if(!["computed","supplied","value"].includes(form.countsMode) || !["omitted","supplied"].includes(form.mostCommonMode))throw new Error("Choose computed or supplied threshold controls.");
    if(form.countsMode==="supplied")fields.push(["counts_json",form.countsJson]);
    if(form.countsMode==="value")fields.push(["counts_value_json",form.countsJson]);
    if(form.mostCommonMode==="supplied")fields.push(["most_common_json",form.mostCommonJson]);
  }
  return fields;
}
export async function requestTableAttributes(sourceFile:File,form:TableAttributesForm,signal?:AbortSignal):Promise<TableAttributesRun> {
  signal?.throwIfAborted();if(!(sourceFile instanceof File))throw new Error("Choose an actual native EventTable JSON file.");
  const fields=tableAttributesFields(form),body=new FormData();body.append("source_file",sourceFile);for(const [key,value]of fields)appendNativeMultipartField(body,key,value);
  const response=await fetch("/api/reference-table-attributes",{method:"POST",body,signal});signal?.throwIfAborted();
  if(response.status!==200 && response.status!==400)throw new Error(`Table attributes returned HTTP ${response.status}.`);
  const responseBytes=new Uint8Array(await response.arrayBuffer());try{new TextDecoder("utf-8",{fatal:true}).decode(responseBytes);}catch{throw new Error("The table attributes response is not safe JSON UTF-8.");}
  const value=await readNativeResponse(new Response(responseBytes),{path:"/api/reference-table-attributes",fields,reference:{name:sourceFile.name,size:sourceFile.size,last_modified:sourceFile.lastModified}});signal?.throwIfAborted();
  if(!isTableAttributesEnvelope(value) || value.http_status!==response.status)throw new Error("The table attributes response has an invalid native envelope.");
  if(value.engine!==form.engine && !(value.http_status===400 && value.engine===null) || value.operation!==form.operation && !(value.http_status===400 && value.operation===null))throw new Error("The table attributes response belongs to another engine or operation.");
  const call=fields.find(([key])=>key==="call_json"),earlyControl=value.http_status===400&&value.phase==="control";
  if(value.call_mode!==(call?"original":"friendly")&&!(earlyControl&&value.call_mode===null))throw new Error("The table attributes response belongs to another call mode.");
  if(call){
    if(value.call_json===null){if(!(earlyControl&&value.call_mode===null))throw new Error("The table attributes response lost the original call.");}
    else{const expected=nativeStringBytes(call[1]),actual=referenceLogFilterStringBytes(value.call_json);if(actual.length!==expected.length||actual.some((byte,index)=>byte!==expected[index]))throw new Error("The table attributes response belongs to another original call.");}
    if(value.call_mode==="original"&&(value.case_id_key!==""||value.attribute_key!==""))throw new Error("The original table attributes response contains a friendly selector role.");
  }else if(value.call_json!==null)throw new Error("The table attributes response contains an unrelated original call.");
  return {value,sourceFile,fields,raw:nativeResponseJson(value),responseBytes};
}
export type TableAttributesArtifact="source"|"source_after"|"result"|"counts"|"call"|"controls_before"|"controls_after";
export function tableAttributesArtifactFile(run:TableAttributesRun,kind:TableAttributesArtifact):File {
  const value=kind==="source"?run.value.source_table_json:kind==="source_after"?run.value.source_after_table_json:kind==="counts"?run.value.counts_table_json:kind==="call"?run.value.call_json:kind==="controls_before"?run.value.controls_before_json:kind==="controls_after"?run.value.controls_after_json:run.value.filtered_table_json;
  if(value===null || (kind==="result" || kind==="counts") && (run.value.http_status!==200 || run.value.error!==null))throw new Error("The native attribute artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)],`${kind}-${run.value.engine??"table"}-attributes.json`,{type:"application/json"});
}
export function tableAttributesResponseFile(run:TableAttributesRun):File{return new File([run.responseBytes],"table-attributes-response.json",{type:"application/json"});}
export function tableAttributesSourceExample(engine:TableRepetitionEngine):File{const file=tableRepetitionSourceExample(engine);return new File([file],`${engine}-attributes-example.json`,{type:file.type});}
export interface TableAttributesIdentity {source:File|null;form:TableAttributesForm;}
export class TableAttributesRequestGate {
  private generation=0;
  issue(value:TableAttributesIdentity){return {generation:++this.generation,source:value.source,form:JSON.stringify(value.form)};}
  invalidate(){++this.generation;}
  accepts(ticket:ReturnType<TableAttributesRequestGate["issue"]>,value:TableAttributesIdentity,signal?:AbortSignal){return !signal?.aborted && ticket.generation===this.generation && ticket.source===value.source && ticket.form===JSON.stringify(value.form);}
}
