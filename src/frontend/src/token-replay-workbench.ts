import {activeMiningSourceTables} from "./active-mining-source";
import {isWorkflowInputTable, type WorkflowInputTable} from "./workflow-input-workbench";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { nativeResponseJson, nativeResponseRequest, readNativeResponse } from "./native-response";
import { type DiagnosticTable } from "./model-diagnostics";

export type TokenReplayAdapter = "log" | "stream" | "dataframe" | "lazyframe";
export interface TokenReplayWorkbenchForm {
  variant: "forward" | "backwards";
  adapter: TokenReplayAdapter;
  inputFormat?: "auto" | "table";
  miningSelectors?: Partial<Record<"case_id_key" | "activity_key" | "timestamp_key" | "resource_key" | "lifecycle_key", string>>;
  importFields: Record<string,string>;
  replayFields: Record<string,string>;
}
export const TOKEN_REPLAY_IMPORT_CONTROLS = [
  ["case_column","CSV case column","case_id"], ["activity_column","CSV activity column","activity"],
  ["timestamp_column","CSV timestamp column","timestamp"], ["resource_column","CSV resource column","resource"],
  ["lifecycle_column","CSV lifecycle column","lifecycle"],
  ["xes_case_attribute","XES case attribute","concept:name"], ["xes_activity_attribute","XES activity attribute","concept:name"],
  ["xes_timestamp_attribute","XES timestamp attribute","time:timestamp"], ["xes_resource_attribute","XES resource attribute","org:resource"],
  ["xes_lifecycle_attribute","XES lifecycle attribute","lifecycle:transition"], ["xes_max_traces","XES trace limit","unlimited"],
  ["xes_attributes_to_read","XES attribute projection (comma-separated)","All scalar attributes"],
  ["xes_max_decompressed_bytes","XES decompressed byte cap","536870912"],
] as const;
export const TOKEN_REPLAY_CONTROLS = [
  ["token_replay_activity_attribute","Replay activity attribute","text","both"],
  ["token_replay_case_id_attribute","Replay case attribute (flat inputs)","text","both"],
  ["token_replay_max_rows_to_materialize","Replay event cap","text","both"],
  ["token_replay_stop_immediately_unfit","Stop at first deviation","boolean","both"],
  ["token_replay_walk_through_hidden_transitions","Route through hidden transitions","boolean","both"],
  ["token_replay_consider_remaining_in_fitness","Remaining tokens affect fitting status","boolean","forward"],
  ["token_replay_try_to_reach_final_marking_through_hidden","Reach final marking through hidden transitions","boolean","forward"],
  ["token_replay_consider_activities_not_in_model_in_fitness","Unknown activities affect fitting status","boolean","forward"],
  ["token_replay_cleaning_token_flood","Clean token flooding","boolean","forward"],
  ["token_replay_exhaustive_invisible_exploration","Exhaustive invisible exploration","boolean","forward"],
  ["token_replay_enable_pltr_fitness","Place and transition fitness maps","boolean","forward"],
  ["token_replay_disable_variants","Disable activity-variant grouping","boolean","forward"],
  ["token_replay_return_names","Return model names and labels","boolean","forward"],
  ["token_replay_places_shortest_path_by_hidden","Hidden-transition path cache (JSON)","json","forward"],
] as const;
const IMPORT_BOOLEAN_KEYS = ["xes_require_timestamp","xes_timestamp_sort","xes_reverse_sort"] as const;
export const TOKEN_REPLAY_XES_BOOLEAN_CONTROLS = [
  ["xes_require_timestamp","Require XES timestamps"], ["xes_timestamp_sort","Sort XES events by timestamp"],
  ["xes_reverse_sort","Reverse XES timestamp sort"],
] as const;
export const defaultTokenReplayWorkbenchForm = (): TokenReplayWorkbenchForm => ({
  variant:"forward", adapter:"log", importFields:{}, replayFields:{token_replay_enable_pltr_fitness:"true"},
});
/** Source selectors are exact strings; this carrier is separate from replay-specific options. */
export function tokenReplayInputFields(form: Pick<TokenReplayWorkbenchForm, "inputFormat" | "miningSelectors">): Array<[string,string]> {
  const fields: Array<[string,string]> = [];
  if (form.inputFormat !== undefined) {if (!["auto", "table"].includes(form.inputFormat)) throw new Error("Select CSV/XES or native EventTable input."); fields.push(["mining_input_format", form.inputFormat]);}
  if (form.miningSelectors !== undefined) {for (const [key, value] of Object.entries(form.miningSelectors)) if (!["case_id_key", "activity_key", "timestamp_key", "resource_key", "lifecycle_key"].includes(key) || typeof value !== "string") throw new Error("Source selectors must map native selector names to exact strings."); fields.push(["mining_selectors_json", JSON.stringify(form.miningSelectors)]);}
  return fields;
}
export function tokenReplayWorkbenchFields(form:TokenReplayWorkbenchForm, logName:string): Array<[string,string]> {
  if (!["forward","backwards"].includes(form.variant)) throw new Error("Select forward or backwards replay.");
  if (!["log","stream","dataframe","lazyframe"].includes(form.adapter)) throw new Error("Select a supported replay input shape.");
  const fields:Array<[string,string]> = [["token_replay_variant",form.variant],["token_replay_input_adapter",form.adapter]];
  fields.push(...tokenReplayInputFields(form));
  if (form.inputFormat !== "table") {
  for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) if (form.importFields[key] !== undefined && form.importFields[key] !== "") fields.push([key,form.importFields[key]]);
  for (const key of IMPORT_BOOLEAN_KEYS) {
    const value = form.importFields[key];
    if (value) { if (!["true","false"].includes(value)) throw new Error(`${key} must be true or false.`); fields.push([key,value]); }
  }
  if (form.importFields.xes_variant) {
    if (!["iterparse","line_by_line","iterparse_20","iterparse_mem_compressed","rustxes","chunk_regex"].includes(form.importFields.xes_variant)) throw new Error("Select a supported XES importer.");
    fields.push(["xes_variant",form.importFields.xes_variant]);
  }
  const delimiter = form.importFields.delimiter || (/\.tsv$/i.test(logName) ? "\t" : "");
  if (delimiter) { if (delimiter.length !== 1) throw new Error("The CSV delimiter must be one character."); fields.push(["delimiter",delimiter]); }
  }
  for (const [key,,kind,scope] of TOKEN_REPLAY_CONTROLS) {
    if (scope === "forward" && form.variant !== "forward") continue;
    const value = form.replayFields[key];
    if (value === undefined || value === "") continue;
    if (kind === "boolean" && !["true","false"].includes(value)) throw new Error(`${key} must be true or false.`);
    if (kind === "json") { try { parseNativeJson(value); } catch { throw new Error("The hidden-transition cache must be valid JSON."); } }
    fields.push([key,value]);
  }
  return fields;
}
const object=(value:unknown):value is Record<string,unknown>=>value!==null&&typeof value==="object"&&!Array.isArray(value);
const index=(value:unknown)=>Number.isSafeInteger(value)&&Number(value)>=0;
const integer=(holder:Record<string,unknown>,key:string)=>{
  const value=nativeCell(holder,key);
  return typeof value==="string" ? /^-?\d+$/.test(value) : typeof value==="number"&&Number.isFinite(value)&&Number.isInteger(value);
};
export function isReplayTaggedValue(value:unknown):boolean {
  if (!object(value)||typeof value.type!=="string") return false;
  switch(value.type) {
    case "str": return typeof value.value==="string";
    case "bool": return typeof value.value==="boolean";
    case "int": return integer(value,"value");
    case "float": return typeof value.hex==="string"&&/^(?:-?(?:inf|0x[01]\.[0-9a-f]+p[+-]\d+)|nan)$/.test(value.hex);
    case "NoneType": return value.value===null;
    case "NativeNA": return true;
    case "NativeDate": return typeof value.value==="string";
    case "bytes": return typeof value.hex==="string"&&/^(?:[0-9a-f]{2})*$/.test(value.hex);
    case "list": case "tuple": case "set": case "frozenset": return Array.isArray(value.values)&&value.values.every(isReplayTaggedValue);
    case "dict": case "Marking": return Array.isArray(value.items)&&value.items.every(pair=>Array.isArray(pair)&&pair.length===2&&(isReplayTaggedValue(pair[0])||index(pair[0]))&&isReplayTaggedValue(pair[1]));
    case "Place": return index(value.index)&&isReplayTaggedValue(value.name);
    case "Transition": return index(value.index)&&isReplayTaggedValue(value.name)&&isReplayTaggedValue(value.label);
    case "Trace": return (value.index===null||index(value.index))&&isReplayTaggedValue(value.attributes)&&Array.isArray(value.events)&&value.events.every(isReplayTaggedValue);
    default:return false;
  }
}
export interface TokenReplayWorkflow {
  family:"TOKEN_REPLAY_WORKFLOW"; variant:"forward"|"backwards";
  input_before?: WorkflowInputTable | null; input_after?: WorkflowInputTable | null; prepared_log?: Record<string, unknown>;
  input_context:{adapter:TokenReplayAdapter;origin:"uploaded_csv_rows"|"uploaded_grouped_log"|"uploaded_typed_table";dataset:string;model:string;
    log_bytes:number;model_bytes:number;source_case_count:number;source_event_count:number;
    evaluated_case_count:number;evaluated_event_count:number;activity_attribute:string;case_id_attribute:string};
  model_context:{places:Array<{index:number;id:string;initial_tokens:number;final_tokens:number;initial_key:boolean;final_key:boolean}>;
    transitions:Array<{index:number;id:string;label:string|null}>};
  full_result:Record<string,unknown>|null;
  diagnostics:{family:"TOKEN_REPLAY";variant:"forward"|"backwards";traces:Record<string,unknown>[]};
}
function taggedReplayRows(value:Record<string,unknown>):unknown[]|undefined {
  if (value.type==="list"&&Array.isArray(value.values)) return value.values;
  if (value.type==="tuple"&&Array.isArray(value.values)&&value.values.length===4&&object(value.values[0])&&value.values[0].type==="list"&&Array.isArray(value.values[0].values)) return value.values[0].values;
  return undefined;
}
export function isTokenReplayWorkflow(value:unknown):value is TokenReplayWorkflow {
  if (!object(value)||value.family!=="TOKEN_REPLAY_WORKFLOW"||!["forward","backwards"].includes(String(value.variant))||!object(value.input_context)||!object(value.model_context)||!object(value.diagnostics)) return false;
  const input=value.input_context,model=value.model_context,diagnostics=value.diagnostics;
  if (["input_before", "input_after"].some(key => value[key] !== undefined && value[key] !== null && !isWorkflowInputTable(value[key])) || value.prepared_log !== undefined && !object(value.prepared_log)) return false;
  if (!["log","stream","dataframe","lazyframe"].includes(String(input.adapter))||!["uploaded_csv_rows","uploaded_grouped_log","uploaded_typed_table"].includes(String(input.origin))||
      !["dataset","model","activity_attribute","case_id_attribute"].every(key=>typeof input[key]==="string")||
      !["log_bytes","model_bytes","source_case_count","source_event_count","evaluated_case_count","evaluated_event_count"].every(key=>index(input[key]))) return false;
  if (!Array.isArray(model.places)||!model.places.every((row,i)=>object(row)&&row.index===i&&typeof row.id==="string"&&integer(row,"initial_tokens")&&integer(row,"final_tokens")&&typeof row.initial_key==="boolean"&&typeof row.final_key==="boolean")||
      !Array.isArray(model.transitions)||!model.transitions.every((row,i)=>object(row)&&row.index===i&&typeof row.id==="string"&&(row.label===null||typeof row.label==="string"))) return false;
  if (diagnostics.family!=="TOKEN_REPLAY"||diagnostics.variant!==value.variant||!Array.isArray(diagnostics.traces)||diagnostics.traces.length!==input.evaluated_case_count) return false;
  if (!diagnostics.traces.every((row,i)=>object(row)&&row.trace_index===i&&typeof row.case_id==="string"&&typeof row.trace_is_fit==="boolean"&&typeof row.trace_fitness==="number"&&Number.isFinite(row.trace_fitness)&&
      ["produced","consumed","missing","remaining"].every(key=>integer(row,key))&&index(row.replayed_events)&&typeof row.replay_stopped==="boolean"&&
      ["activated_transitions","transitions_with_problems","enabled_transitions","unknown_event_indices"].every(key=>Array.isArray(row[key])&&(row[key] as unknown[]).every(index))&&
      Array.isArray(row.reached_marking)&&row.reached_marking.every((_,j)=>integer(row.reached_marking as unknown as Record<string,unknown>,String(j)))&&Array.isArray(row.places)&&Array.isArray(row.event_diagnostics))) return false;
  if (value.variant==="backwards") return value.full_result===null;
  if (!object(value.full_result)||!isReplayTaggedValue(value.full_result)) return false;
  const rows=taggedReplayRows(value.full_result);
  return !!rows&&rows.length===diagnostics.traces.length&&rows.every(row=>object(row)&&row.type==="dict");
}
/** Traverse tagged dictionaries as ordered entries, retaining object keys and exact scalar carriers. */
export function replayTaggedTable(value:Record<string,unknown>):DiagnosticTable {
  const rows:unknown[][]=[];const records:unknown[]=[];
  const visit=(node:unknown,path:string,key?:unknown)=>{
    if (!object(node)) return;
    records.push(node);rows.push([path,key,node.type,node.hex,integer(node,"value")?nativeCell(node,"value"):node.value,node.index]);
    if (Array.isArray(node.values)) node.values.forEach((child,i)=>visit(child,`${path}/values/${i}`));
    if (Array.isArray(node.items)) node.items.forEach((pair,i)=>{if(Array.isArray(pair)){visit(pair[0],`${path}/items/${i}/0`);visit(pair[1],`${path}/items/${i}/1`,pair[0]);}});
    for(const property of ["name","label","attributes"]) if(object(node[property]))visit(node[property],`${path}/${property}`);
    if(Array.isArray(node.events))node.events.forEach((child,i)=>visit(child,`${path}/events/${i}`));
  };
  visit(value,"/full_result");return {id:"tagged_values",title:"Every typed public value and dictionary entry",columns:["path","dictionary_key","type","float_hex","value","index"],rows,records};
}
export function tokenReplayWorkbenchTables(value:TokenReplayWorkflow,fields?:Array<[string,string]>):DiagnosticTable[] {
  const tables=activeMiningSourceTables(value);
  if(value.full_result){
    tables.push(replayTaggedTable(value.full_result));
    const rows=taggedReplayRows(value.full_result)??[];
    const keys=[...new Set(rows.flatMap(row=>object(row)&&Array.isArray(row.items)?row.items.map(pair=>Array.isArray(pair)&&object(pair[0])&&typeof pair[0].value==="string"?pair[0].value:""):[]))];
    tables.push({id:"public_trace_rows",title:"Complete public replay trace rows",columns:["row_index",...keys],records:rows,
      rows:rows.map((row,i)=>[i,...keys.map(key=>object(row)&&Array.isArray(row.items)?row.items.find(pair=>Array.isArray(pair)&&object(pair[0])&&pair[0].value===key)?.[1]:undefined)])});
  }
  const submitted=fields??nativeResponseRequest(value)?.fields;
  if(submitted)tables.push({id:"request",title:"Submitted replay options",columns:["field","value"],rows:submitted});
  return tables;
}
export interface TokenReplayWorkbenchRun {
  value:TokenReplayWorkflow;raw:string;fields:Array<[string,string]>;tables:DiagnosticTable[];
  inputs:{log:File;model:File;log_sha256:string;model_sha256:string};
}
async function fileHash(file:File):Promise<string>{return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256",await file.arrayBuffer())),byte=>byte.toString(16).padStart(2,"0")).join("");}
export async function runTokenReplayWorkbench(log:File,model:File,form:TokenReplayWorkbenchForm,signal?:AbortSignal):Promise<TokenReplayWorkbenchRun>{
  const fields=tokenReplayWorkbenchFields(form,log.name);const [logHash,modelHash]=await Promise.all([fileHash(log),fileHash(model)]);
  signal?.throwIfAborted();const body=new FormData();body.append("log",log);body.append("model",model);fields.forEach(([key,value])=>body.append(key,value));
  const path="/api/replay-token-workbench";const response=await fetch(path,{method:"POST",body,signal});
  const value=await readNativeResponse(response,{path,fields});
  signal?.throwIfAborted();
  if(!response.ok)throw new Error(object(value)&&typeof value.error==="string"?value.error:`Token replay failed (${response.status}).`);
  if(!isTokenReplayWorkflow(value))throw new Error("The server did not return a valid native token-replay workflow.");
  const input=value.input_context;
  if(value.variant!==form.variant||input.adapter!==form.adapter||input.dataset!==log.name||input.model!==model.name||input.log_bytes!==log.size||input.model_bytes!==model.size||
      (value.variant==="forward"&&(value.full_result?.type==="tuple")!==(form.replayFields.token_replay_enable_pltr_fitness==="true")))throw new Error("The replay response does not match the submitted inputs and options.");
  if (form.inputFormat === "table" && input.origin !== "uploaded_typed_table") throw new Error("The native result did not retain the uploaded typed source.");
  return {value,raw:nativeResponseJson(value),fields,tables:tokenReplayWorkbenchTables(value,fields),inputs:{log,model,log_sha256:logHash,model_sha256:modelHash}};
}
export function tokenReplayWorkbenchReport(run:TokenReplayWorkbenchRun):string{
  const metadata=(file:File,sha256:string)=>({name:file.name,size:file.size,last_modified:file.lastModified,sha256});
  return nativeJson({family:"TOKEN_REPLAY_WORKFLOW_REPORT",request:nativeResponseRequest(run.value),
    inputs:{log:metadata(run.inputs.log,run.inputs.log_sha256),model:metadata(run.inputs.model,run.inputs.model_sha256)},native_result:run.value},2);
}
