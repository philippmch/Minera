import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { validStreamingResult, streamingQuery, streamingTables, defaultStreaming, type StreamingForm, type StreamingMethod, type StreamingResult } from "./streaming-workspace";
import { type OcelPreparedTable } from "./ocel-preparation";
import { validDeclareStreamEvent, type DeclareStreamEvent } from "./declare-stream-workspace";

export const LIVE_XES_VARIANTS = ["iterparse", "line_by_line", "iterparse_mem_compressed", "iterparse_20", "chunk_regex", "rustxes"] as const;
export interface LiveImportForm {
  delimiter: string; trim: boolean; header: boolean; caseAliases: boolean; resource: string; lifecycle: string;
  xesVariant: typeof LIVE_XES_VARIANTS[number]; xesCase: string; xesActivity: string; xesTimestamp: string; xesResource: string; xesLifecycle: string;
}
export function defaultLiveImport(): LiveImportForm { return { delimiter: ",", trim: true, header: true, caseAliases: false, resource: "", lifecycle: "", xesVariant: "iterparse", xesCase: "concept:name", xesActivity: "concept:name", xesTimestamp: "time:timestamp", xesResource: "org:resource", xesLifecycle: "lifecycle:transition" }; }
export interface LiveSource { source_id: string; name: string; format: "csv" | "xes" | "pnml" | "declare_json"; size: number; provided_sha256: string; }
export interface LiveBatch { batch_id: string; first_position: number; events: number; }
export interface LiveDelivery { position: number; source_position: number; batch_id: string; case_id: string; activity: string; accepted: boolean; event_index: number; error: string | null; native_event?: DeclareStreamEvent; }
export interface LiveTermination { case_id: string; revision: number; is_fit: boolean; missing?: number; remaining?: number; marking?: number[]; }
export interface LiveSnapshot {
  family: "LIVE_STREAM_SESSION"; session_id: string; method: StreamingMethod; state: "INACTIVE" | "ACTIVE" | "FINISHED";
  revision: number; pending: number; enqueued: number; delivered: number; deliveries_offset: number; registered: boolean; observer_id: number | null;
  result: StreamingResult; batches: LiveBatch[]; deliveries: LiveDelivery[]; terminations: LiveTermination[];
  configuration: Record<string,string>; sources: LiveSource[]; released?: boolean;
}
export interface LiveRun { result: LiveSnapshot; raw: string; report_json: string; tables: Record<string,OcelPreparedTable>; }
const record = (v: unknown): v is Record<string,unknown> => !!v && typeof v === "object" && !Array.isArray(v);
const count = (v: unknown): v is number => Number.isSafeInteger(v) && (v as number) >= 0;
export function validLiveSnapshot(value: unknown): value is LiveSnapshot {
  if (!record(value) || value.family !== "LIVE_STREAM_SESSION" || typeof value.session_id !== "string" || !value.session_id || !["dfg","footprints","temporal","tbr","declare"].includes(String(value.method)) || !["INACTIVE","ACTIVE","FINISHED"].includes(String(value.state))) return false;
  if (![value.revision,value.pending,value.enqueued,value.delivered].every(count) || typeof value.registered !== "boolean" || !(value.observer_id === null || count(value.observer_id)) || value.registered !== (value.observer_id !== null) || (value.released !== undefined && typeof value.released !== "boolean")) return false;
  if ((value.observer_id !== null && value.observer_id === 0) || (value.state === "FINISHED" && value.pending !== 0) || (value.pending as number) > (value.enqueued as number) - (value.delivered as number)) return false;
  if (!record(value.configuration) || !Object.values(value.configuration).every(v => typeof v === "string") || value.configuration.method !== value.method || value.configuration.session_id !== value.session_id || !record(value.result) || !record(value.result.input)) return false;
  const form = defaultStreaming(); form.method = value.method as StreamingMethod; form.order = value.result.input.event_order as StreamingForm["order"]; form.variant = value.result.variant === "BACKWARDS" ? "backwards" : "forward";
  if (!validStreamingResult(value.result,form) || value.result.input.parsed_events !== value.enqueued) return false;
  if (!Array.isArray(value.batches) || !value.batches.every(b => record(b) && typeof b.batch_id === "string" && b.batch_id && count(b.first_position) && count(b.events))) return false;
  const batches = value.batches as LiveBatch[]; let position = 0;
  for (const batch of batches) { if (batch.first_position !== position) return false; position += batch.events; }
  if (position !== value.enqueued || new Set(batches.map(b => b.batch_id)).size !== batches.length) return false;
  // Responses list the latest deliveries only: deliveries_offset is the index of the first listed one.
  if (!count(value.deliveries_offset) || !Array.isArray(value.deliveries) || (value.deliveries_offset as number) + value.deliveries.length !== value.delivered || value.deliveries.length > (value.enqueued as number) || !value.deliveries.every(d => record(d) && count(d.position) && d.position < (value.enqueued as number) && count(d.source_position) && count(d.event_index) && batches.some(b => b.batch_id === d.batch_id && (d.position as number) >= b.first_position && (d.position as number) < b.first_position+b.events) && typeof d.case_id === "string" && typeof d.activity === "string" && typeof d.accepted === "boolean" && (d.error === null || typeof d.error === "string"))) return false;
  if (new Set(value.deliveries.map(d => (d as LiveDelivery).position)).size !== value.deliveries.length) return false;
  if(value.result.family==="STREAMING_DECLARE") {const model=value.result.model; if(!value.deliveries.every(d=>d.error!==null&&d.native_event===undefined||validDeclareStreamEvent(d.native_event,model)&&d.native_event.event_index===d.event_index&&d.native_event.activity===d.activity))return false;}
  if (!Array.isArray(value.terminations) || !value.terminations.every(t => record(t) && typeof t.case_id === "string" && count(t.revision) && t.revision <= (value.revision as number) && typeof t.is_fit === "boolean" && (value.method !== "tbr" || Number.isSafeInteger(t.missing) && Number.isSafeInteger(t.remaining) && Array.isArray(t.marking) && t.marking.every(count)))) return false;
  if (!Array.isArray(value.sources) || !value.sources.every(s => record(s) && typeof s.source_id === "string" && typeof s.name === "string" && ["csv","xes","pnml","declare_json"].includes(String(s.format)) && count(s.size) && typeof s.provided_sha256 === "string")) return false;
  const sources = value.sources as LiveSource[];
  const modelFormat=value.method==="tbr"?"pnml":value.method==="declare"?"declare_json":null;
  return new Set(sources.map(s => s.source_id)).size === sources.length && sources.length === batches.length + (modelFormat ? 1 : 0) && batches.every(b => sources.some(s => s.source_id === b.batch_id && ["csv","xes"].includes(s.format))) && (!modelFormat || sources.some(s => s.source_id === "model" && s.format === modelFormat));
}
export function liveTables(snapshot: LiveSnapshot): Record<string,OcelPreparedTable> {
  const tables = streamingTables(snapshot.result);
  const table = (columns: string[], items: object[], rowAt: (item: object) => unknown[]): OcelPreparedTable => ({ columns, count: items.length, rowAt: i => rowAt(items[i]) });
  const columns = ["session_id","method","state","revision","pending","enqueued","delivered","deliveries_offset","registered","observer_id","released"];
  tables.session = { columns, count: 1, rowAt: () => columns.map(key => nativeCell(snapshot,key) ?? (key === "released" ? false : null)) };
  tables.batches = table(["batch_id","first_position","events"],snapshot.batches,item => ["batch_id","first_position","events"].map(key => nativeCell(item,key)));
  const deliveryColumns = ["position","source_position","batch_id","case_id","activity","accepted","event_index","error"];
  tables.deliveries = table(deliveryColumns,snapshot.deliveries,item => deliveryColumns.map(key => nativeCell(item,key)));
  if(snapshot.result.family==="STREAMING_DECLARE") {
    const rows=snapshot.deliveries.filter(d=>d.native_event).map(d=>({delivery:d,event:d.native_event!}));
    const columns=["position","source_position","batch_id","case_id","activity","timestamp","event_index","deviations"];
    tables.declare_events={columns,count:rows.length,rowAt:i=>{const r=rows[i];return [nativeCell(r.delivery,"position"),nativeCell(r.delivery,"source_position"),r.delivery.batch_id,r.event.case_id,r.event.activity,r.event.timestamp,nativeCell(r.event,"event_index"),nativeCell(r.event,"deviations")];}};
    const violations=rows.flatMap(r=>r.event.violated_constraints.map(rule=>({row:r,rule}))),names=["constraint_index","template","first","second","binary"];
    tables.event_violations={columns:["position","batch_id","case_id","timestamp","event_index",...names],count:violations.length,rowAt:i=>{const v=violations[i];return [nativeCell(v.row.delivery,"position"),v.row.delivery.batch_id,v.row.event.case_id,v.row.event.timestamp,nativeCell(v.row.event,"event_index"),...names.map(key=>nativeCell(v.rule,key))];}};
  }
  tables.terminations = table(["case_id","revision","is_fit","missing","remaining","marking"],snapshot.terminations,item => [nativeCell(item,"case_id"),nativeCell(item,"revision"),nativeCell(item,"is_fit"),nativeCell(item,"missing") ?? null,nativeCell(item,"remaining") ?? null,Array.isArray((item as LiveTermination).marking) ? nativeJson((item as LiveTermination).marking) : null]);
  const sourceColumns = ["source_id","name","format","size","provided_sha256"];
  tables.sources = table(sourceColumns,snapshot.sources,item => sourceColumns.map(key => nativeCell(item,key)));
  tables.configuration = { columns: ["parameter","value"],count: Object.keys(snapshot.configuration).length,rowAt: i => {const key=Object.keys(snapshot.configuration)[i];return [key,snapshot.configuration[key]];} };
  return tables;
}
export function liveCreateQuery(form: StreamingForm, imports: LiveImportForm, sessionId: string): URLSearchParams {
  const params = new URLSearchParams(streamingQuery({...form,finalize:false})); params.set("method",form.method);params.set("session_id",sessionId);
  if (imports.delimiter.length !== 1 || /[\r\n\0]/.test(imports.delimiter)) throw new Error("CSV delimiter must be one character.");
  params.set("csv_delimiter",imports.delimiter);params.set("csv_trim_fields",String(imports.trim));params.set("csv_has_header",String(imports.header));params.set("csv_case_aliases",String(imports.caseAliases));
  if(imports.resource)params.set("csv_resource_column",imports.resource);if(imports.lifecycle)params.set("csv_lifecycle_column",imports.lifecycle);
  params.set("xes_variant",imports.xesVariant);
  for (const [key,value] of [["xes_case_attribute",imports.xesCase],["xes_activity_attribute",imports.xesActivity],["xes_timestamp_attribute",imports.xesTimestamp],["xes_resource_attribute",imports.xesResource],["xes_lifecycle_attribute",imports.xesLifecycle]]) {if(!value)throw new Error(key+" must not be empty.");params.set(key,value);}
  return params;
}
async function digest(file: Blob): Promise<string> {return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256",await file.arrayBuffer())),v=>v.toString(16).padStart(2,"0")).join("");}
async function liveRequest(route: string, params: URLSearchParams, method: "GET" | "POST", body?: FormData, signal?: AbortSignal): Promise<LiveRun> {
  signal?.throwIfAborted();const response=await fetch("/api/live-stream/"+route+"?"+params,{method,...(body?{body}:{}),...(signal?{signal}:{})});const raw=await response.text();signal?.throwIfAborted();let value:unknown;
  try{value=parseNativeJson(raw);}catch{throw new Error("The backend returned invalid live-stream JSON.");}
  if(record(value)&&"error"in value)throw new Error(String(value.error));if(!response.ok)throw new Error("Live streaming returned "+response.status+".");
  if(!validLiveSnapshot(value)||value.session_id!==params.get("session_id"))throw new Error("The backend returned an invalid or different live session.");
  return {result:value,raw,tables:liveTables(value),report_json:'{"family":"LIVE_STREAM_REPORT","request":'+JSON.stringify({route,method,parameters:Object.fromEntries(params)})+',"response":'+raw+'}'};
}
export async function createLiveSession(form: StreamingForm, imports: LiveImportForm, id: string, model?: File | null, signal?: AbortSignal): Promise<LiveRun> {
  const params=liveCreateQuery(form,imports,id);let body:FormData|undefined;
  if(form.method==="tbr"||form.method==="declare"){if(!model)throw new Error("Choose a model.");body=new FormData();body.append("model",model,model.name);params.set("source_name",model.name);params.set("source_sha256",await digest(model));}
  const run=await liveRequest("create",params,"POST",body,signal);if(run.result.method!==form.method)throw new Error("The session has a different monitor.");return run;
}
export function getLiveSession(id: string, signal?: AbortSignal): Promise<LiveRun> {return liveRequest("snapshot",new URLSearchParams({session_id:id}),"GET",undefined,signal);}
export async function appendLiveSession(run: LiveRun, file: File, format: "csv" | "xes", batchId: string, wait: boolean, signal?: AbortSignal): Promise<LiveRun> {
  const params=new URLSearchParams({session_id:run.result.session_id,expected_revision:String(nativeCell(run.result,"revision")),batch_id:batchId,format,source_name:file.name,source_sha256:await digest(file),wait_until_drained:String(wait)}),body=new FormData();body.append("log",file,file.name);return liveRequest("append",params,"POST",body,signal);
}
export type LiveAction = "start"|"drain"|"finish"|"register"|"deregister"|"reset_all"|"reset_case"|"terminate_case"|"terminate_all"|"release";
export function actLiveSession(run: LiveRun, action: LiveAction, caseId: string, signal?: AbortSignal): Promise<LiveRun> {
  const params=new URLSearchParams({session_id:run.result.session_id,expected_revision:String(nativeCell(run.result,"revision"))});if(action!=="release")params.set("action",action);if(caseId&&action!=="release")params.set("case_id",caseId);
  return liveRequest(action==="release"?"release":"action",params,"POST",undefined,signal);
}
export async function listLiveSessions(signal?: AbortSignal): Promise<LiveSnapshot[]> {
  const response=await fetch("/api/live-stream/list",{...(signal?{signal}:{})});const raw=await response.text();signal?.throwIfAborted();const value=parseNativeJson(raw);
  if(!response.ok||!record(value)||!Array.isArray(value.sessions)||!value.sessions.every(validLiveSnapshot)||new Set(value.sessions.map(s=>s.session_id)).size!==value.sessions.length)throw new Error("Could not list valid server stream sessions.");return value.sessions;
}
export async function liveOriginal(run: LiveRun, source: LiveSource, signal?: AbortSignal): Promise<Blob> {
  const response=await fetch("/api/live-stream/source?"+new URLSearchParams({session_id:run.result.session_id,source_id:source.source_id}),{...(signal?{signal}:{})});const blob=await response.blob();signal?.throwIfAborted();
  if(!response.ok||blob.size!==source.size||source.provided_sha256&&await digest(blob)!==source.provided_sha256)throw new Error("The original live upload is unavailable or its byte binding differs.");return blob;
}
export function formFromLive(snapshot: LiveSnapshot): StreamingForm {
  const form=defaultStreaming(),p=snapshot.configuration,prefix=snapshot.method==="dfg"?"streaming_":"streaming_"+snapshot.method+"_";form.method=snapshot.method;form.order=snapshot.result.input.event_order;
  form.caseColumn=p[prefix+"case_id_column"]??"";form.activityColumn=p[prefix+"activity_column"]??"";form.timestampColumn=p[prefix+"timestamp_column"]??"";form.startColumn=p[prefix+"start_timestamp_column"]??"";form.zeta=p[prefix+"zeta"]??"";form.maxInvisible=p[prefix+"max_invisible"]??"";form.variant=p[prefix+"variant"]==="backwards"?"backwards":"forward";form.stopImmediately=p[prefix+"stop_immediately"]==="true";
  if(snapshot.method==="footprints") {
    for(const key of ["activities","starts","ends"] as const){const suffix=key==="starts"?"start_activities":key==="ends"?"end_activities":key;form[key]=p[prefix+suffix]?JSON.stringify(p[prefix+suffix].split(",").map(v=>v.trim())):"";}
    for(const key of ["sequence","parallel"] as const)form[key]=p[prefix+key]?JSON.stringify(p[prefix+key].split(",").map(v=>v.split("->"))):"";
  }
  if(p[prefix+"profile"])form.profile=JSON.stringify(p[prefix+"profile"].split(",").map(v=>{const [pair,mean,sd]=v.split(":");return [...pair.split("->"),Number(mean),Number(sd)];}));return form;
}
export function importsFromLive(snapshot: LiveSnapshot): LiveImportForm {
  const p=snapshot.configuration,form=defaultLiveImport();form.delimiter=p.csv_delimiter??form.delimiter;form.trim=p.csv_trim_fields!=="false";form.header=p.csv_has_header!=="false";form.caseAliases=p.csv_case_aliases==="true";form.resource=p.csv_resource_column??"";form.lifecycle=p.csv_lifecycle_column??"";form.xesVariant=(p.xes_variant as LiveImportForm["xesVariant"])??form.xesVariant;
  form.xesCase=p.xes_case_attribute??form.xesCase;form.xesActivity=p.xes_activity_attribute??form.xesActivity;form.xesTimestamp=p.xes_timestamp_attribute??form.xesTimestamp;form.xesResource=p.xes_resource_attribute??form.xesResource;form.xesLifecycle=p.xes_lifecycle_attribute??form.xesLifecycle;return form;
}
