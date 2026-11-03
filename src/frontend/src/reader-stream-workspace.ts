import { isLogConversionResult, type LogConversionLog, type LogConversionResult } from "./types";
import { backendFetch } from "./backend-fetch";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { LIVE_XES_VARIANTS } from "./live-stream-workspace";
import { type OcelPreparedTable } from "./ocel-preparation";
import { type CsvOptions } from "./api";

export const READER_KINDS = [["csv_events","CSV event reader"],["xes_events","XES event reader"],["xes_traces","XES whole-trace reader"]] as const;
export type ReaderKind = typeof READER_KINDS[number][0];
type NativeItem = Extract<LogConversionResult,{log:LogConversionLog}>;
export interface ReaderItem {position:number;generation:number;reader_position:number;batch_id:string;native_item:NativeItem;}
export interface ReaderDelivery {position:number;native_item:NativeItem;}
export interface ReaderBatch {batch_id:string;generation:number;reader_position:number;first_position:number;items:number;}
export interface ReaderSnapshot {
  family:"STREAM_READER_SESSION";kind:ReaderKind;session_id:string;state:"INACTIVE"|"ACTIVE"|"FINISHED";
  revision:number;pending:number;enqueued:number;delivered:number;registered:boolean;observer_id:number|null;
  reader_generation:number;reader_position:number;exhausted:boolean;items:ReaderItem[];deliveries:ReaderDelivery[];batches:ReaderBatch[];
  configuration:Record<string,string>;source:{name:string;format:"csv"|"xes";size:number;provided_sha256:string};released?:boolean;
}
export interface ReaderForm {
  kind:ReaderKind;caseColumn:string;activityColumn:string;timestampColumn:string;resourceColumn:string;lifecycleColumn:string;
  delimiter:string;trim:boolean;caseAliases:boolean;xesVariant:typeof LIVE_XES_VARIANTS[number];
  xesCase:string;xesActivity:string;xesTimestamp:string;xesResource:string;xesLifecycle:string;
  timestampSort:boolean;reverseSort:boolean;maxTraces:string;prefix:string;
  acceptCase:boolean;caseId:string;acceptActivity:boolean;activity:string;minimum:string;maximum:string;
}
export function defaultReaderForm():ReaderForm {return {kind:"csv_events",caseColumn:"",activityColumn:"",timestampColumn:"",resourceColumn:"",lifecycleColumn:"",delimiter:",",trim:true,caseAliases:false,xesVariant:"iterparse",xesCase:"concept:name",xesActivity:"concept:name",xesTimestamp:"time:timestamp",xesResource:"org:resource",xesLifecycle:"lifecycle:transition",timestampSort:false,reverseSort:false,maxTraces:"",prefix:"",acceptCase:false,caseId:"",acceptActivity:false,activity:"",minimum:"",maximum:""};}
const record=(v:unknown):v is Record<string,unknown>=>!!v&&typeof v==="object"&&!Array.isArray(v);
const count=(v:unknown):v is number=>Number.isSafeInteger(v)&&(v as number)>=0;
function validItem(v:unknown,kind:ReaderKind):v is NativeItem {
  return isLogConversionResult(v)&&v.variant==="TO_EVENT_LOG"&&"log"in v&&v.log.traces.length===1&&(kind==="xes_traces"||v.log.traces[0].events.length===1);
}
export function validReaderSnapshot(v:unknown):v is ReaderSnapshot {
  if(!record(v)||v.family!=="STREAM_READER_SESSION"||!READER_KINDS.some(([id])=>id===v.kind)||typeof v.session_id!=="string"||!v.session_id||!["INACTIVE","ACTIVE","FINISHED"].includes(String(v.state)))return false;
  if(![v.revision,v.pending,v.enqueued,v.delivered,v.reader_generation,v.reader_position].every(count)||typeof v.exhausted!=="boolean"||typeof v.registered!=="boolean"||!(v.observer_id===null||count(v.observer_id)&&v.observer_id>0)||v.registered!==(v.observer_id!==null)||v.released!==undefined&&typeof v.released!=="boolean")return false;
  const kind=v.kind as ReaderKind;
  if(!record(v.configuration)||!Object.values(v.configuration).every(x=>typeof x==="string")||v.configuration.session_id!==v.session_id||v.configuration.kind!==kind||!record(v.source)||typeof v.source.name!=="string"||v.source.format!==(kind==="csv_events"?"csv":"xes")||!count(v.source.size)||typeof v.source.provided_sha256!=="string")return false;
  if(!Array.isArray(v.items)||v.items.length!==v.enqueued||!v.items.every((x,i)=>record(x)&&x.position===i&&count(x.generation)&&x.generation<=Number(v.reader_generation)&&count(x.reader_position)&&typeof x.batch_id==="string"&&validItem(x.native_item,kind)))return false;
  if(!Array.isArray(v.deliveries)||v.deliveries.length!==v.delivered||Number(v.delivered)>Number(v.enqueued)||!v.deliveries.every((x,i)=>record(x)&&x.position===i&&validItem(x.native_item,kind)))return false;
  if(Number(v.pending)>Number(v.enqueued)-Number(v.delivered)||v.state==="FINISHED"&&v.pending!==0)return false;
  if(!Array.isArray(v.batches)||!v.batches.every(x=>record(x)&&typeof x.batch_id==="string"&&!!x.batch_id&&[x.generation,x.reader_position,x.first_position,x.items].every(count)))return false;
  const items=v.items as ReaderItem[],batches=v.batches as ReaderBatch[];let first=0;
  for(const batch of batches){if(batch.first_position!==first||batch.generation>Number(v.reader_generation))return false;for(let i=0;i<batch.items;i++){const item=items[first+i];if(!item||item.batch_id!==batch.batch_id||item.generation!==batch.generation||item.reader_position!==batch.reader_position+i)return false;}first+=batch.items;}
  return first===v.enqueued&&new Set(batches.map(x=>x.batch_id)).size===batches.length;
}
function size(value:string,label:string,maximum=Number.MAX_SAFE_INTEGER,minimum=0):number {const n=Number(value);if(!/^\d+$/.test(value)||!Number.isSafeInteger(n)||n<minimum||n>maximum)throw new Error(`${label} must be an integer from ${minimum} through ${maximum}.`);return n;}
export function readerCreateQuery(form:ReaderForm,id:string):URLSearchParams {
  if(!id)throw new Error("Reader session ID is empty.");if(!READER_KINDS.some(([kind])=>kind===form.kind))throw new Error("Unknown reader kind.");
  const q=new URLSearchParams({session_id:id,kind:form.kind});
  if(form.kind==="csv_events"){
    if(form.delimiter.length!==1||/[\r\n\0]/.test(form.delimiter))throw new Error("CSV delimiter must be one character.");q.set("csv_delimiter",form.delimiter);q.set("csv_trim_fields",String(form.trim));q.set("csv_case_aliases",String(form.caseAliases));
    for(const [field,key] of [["caseColumn","case_id_column"],["activityColumn","activity_column"],["timestampColumn","timestamp_column"],["resourceColumn","csv_resource_column"],["lifecycleColumn","csv_lifecycle_column"]] as const)if(form[field])q.set(key,form[field]);
  }else{
    if(!LIVE_XES_VARIANTS.includes(form.xesVariant))throw new Error("Unknown XES parser.");q.set("xes_variant",form.xesVariant);
    for(const [field,key] of [["xesCase","xes_case_attribute"],["xesActivity","xes_activity_attribute"],["xesTimestamp","xes_timestamp_attribute"],["xesResource","xes_resource_attribute"],["xesLifecycle","xes_lifecycle_attribute"]] as const)q.set(key,form[field]);
    q.set("xes_timestamp_sort",String(form.timestampSort));q.set("xes_reverse_sort",String(form.reverseSort));if(form.maxTraces)q.set("xes_max_traces",String(size(form.maxTraces,"Maximum imported traces")));
  }
  if(form.prefix)q.set("activity_prefix",form.prefix);if(form.acceptCase)q.set("accept_case",form.caseId);if(form.acceptActivity)q.set("accept_activity",form.activity);
  if(form.kind==="xes_traces"){if(form.minimum)q.set("min_trace_events",String(size(form.minimum,"Minimum trace events")));if(form.maximum)q.set("max_trace_events",String(size(form.maximum,"Maximum trace events")));if(form.minimum&&form.maximum&&Number(form.minimum)>Number(form.maximum))throw new Error("Minimum trace events exceeds maximum.");}
  return q;
}
export function readerFormFrom(snapshot:ReaderSnapshot):ReaderForm {
  const form=defaultReaderForm(),q=snapshot.configuration;form.kind=snapshot.kind;
  for(const [field,key] of [["caseColumn","case_id_column"],["activityColumn","activity_column"],["timestampColumn","timestamp_column"],["resourceColumn","csv_resource_column"],["lifecycleColumn","csv_lifecycle_column"],["xesCase","xes_case_attribute"],["xesActivity","xes_activity_attribute"],["xesTimestamp","xes_timestamp_attribute"],["xesResource","xes_resource_attribute"],["xesLifecycle","xes_lifecycle_attribute"],["prefix","activity_prefix"],["caseId","accept_case"],["activity","accept_activity"],["minimum","min_trace_events"],["maximum","max_trace_events"],["maxTraces","xes_max_traces"]] as const)form[field]=q[key]??form[field];
  form.delimiter=q.csv_delimiter??form.delimiter;form.trim=q.csv_trim_fields!=="false";form.caseAliases=q.csv_case_aliases==="true";form.xesVariant=(q.xes_variant as ReaderForm["xesVariant"])??form.xesVariant;form.timestampSort=q.xes_timestamp_sort==="true";form.reverseSort=q.xes_reverse_sort==="true";form.acceptCase="accept_case"in q;form.acceptActivity="accept_activity"in q;return form;
}
export function readerExportImportOptions(form:ReaderForm):CsvOptions {
  return form.kind==="csv_events"?{xesCaseAttribute:form.caseColumn||"case_id",xesActivityAttribute:form.activityColumn||"activity",xesTimestampAttribute:form.timestampColumn||"timestamp",xesResourceAttribute:form.resourceColumn||"resource",xesLifecycleAttribute:form.lifecycleColumn||"lifecycle"}:{xesCaseAttribute:form.xesCase,xesActivityAttribute:form.xesActivity,xesTimestampAttribute:form.xesTimestamp,xesResourceAttribute:form.xesResource,xesLifecycleAttribute:form.xesLifecycle};
}
export function readerTables(s:ReaderSnapshot):Record<string,OcelPreparedTable> {
  const result:Record<string,OcelPreparedTable>={};const table=(columns:string[],rows:unknown[][]):OcelPreparedTable=>({columns,count:rows.length,rowAt:i=>rows[i]});
  const itemRows=s.items.map(x=>[nativeCell(x,"position"),nativeCell(x,"generation"),nativeCell(x,"reader_position"),x.batch_id,x.native_item.log.traces[0].case_id,x.native_item.log.traces[0].events.length,nativeJson(x.native_item)]);
  result.read_items=table(["read_position","generation","reader_position","batch_id","case_id","events","native_item_json"],itemRows);
  result.deliveries=table(["delivery_position","case_id","events","native_item_json"],s.deliveries.map(x=>[nativeCell(x,"position"),x.native_item.log.traces[0].case_id,x.native_item.log.traces[0].events.length,nativeJson(x.native_item)]));
  for(const [name,items] of [["read_events",s.items],["delivered_events",s.deliveries]] as const){const rows=items.flatMap(x=>x.native_item.log.traces[0].events.map((e,i)=>[nativeCell(x,"position"),i,e.case_id,e.activity,e.timestamp,e.resource,e.lifecycle,nativeCell(e,"input_order"),nativeJson(e)]));result[name]=table(["item_position","event_position","case_id","activity","timestamp","resource","lifecycle","source_input_order","native_event_json"],rows);}
  const attributes:unknown[][]=[];
  for(const x of s.items){const trace=x.native_item.log.traces[0];const groups=[{scope:"log",event:null,values:x.native_item.log.attributes},{scope:"trace",event:null,values:trace.attributes},{scope:"trace_properties",event:null,values:trace.properties??[]},...trace.events.map((e,i)=>({scope:"event",event:i,values:e.attributes}))];for(const group of groups)group.values.forEach((a,i)=>attributes.push([nativeCell(x,"position"),group.scope,group.event,i,a.key,a.kind,nativeCell(a,"value"),a.null??false,nativeJson(a)]));}
  result.read_attributes=table(["item_position","scope","event_position","attribute_position","key","kind","value","null","native_attribute_json"],attributes);
  const columns=["batch_id","generation","reader_position","first_position","items"];result.batches=table(columns,s.batches.map(x=>columns.map(k=>nativeCell(x,k))));
  const summary=["session_id","kind","state","revision","reader_generation","reader_position","exhausted","pending","enqueued","delivered","registered","observer_id","released"];result.session=table(summary,[summary.map(k=>nativeCell(s,k)??(k==="released"?false:null))]);
  result.configuration=table(["parameter","value"],Object.entries(s.configuration));const source=["name","format","size","provided_sha256"];result.source=table(source,[source.map(k=>nativeCell(s.source,k))]);return result;
}
export interface ReaderRun {result:ReaderSnapshot;raw:string;report_json:string;tables:Record<string,OcelPreparedTable>;}
async function error(response:Response):Promise<never>{const raw=await response.text();let value:unknown;try{value=parseNativeJson(raw);}catch{throw new Error(`Reader request returned ${response.status}.`);}if(record(value)&&"error"in value)throw new Error(String(value.error));throw new Error(`Reader request returned ${response.status}.`);}
async function request(route:string,query:URLSearchParams,method:"GET"|"POST",body?:FormData,signal?:AbortSignal):Promise<ReaderRun> {
  const response=await backendFetch(`/api/stream-reader/${route}?${query}`,{method,...(body?{body}:{}),...(signal?{signal}:{})});if(!response.ok)return error(response);const raw=await response.text();let v:unknown;try{v=parseNativeJson(raw);}catch{throw new Error("The backend returned invalid reader JSON.");}if(!validReaderSnapshot(v))throw new Error("The backend returned an invalid native reader snapshot.");if(v.session_id!==query.get("session_id"))throw new Error("The backend returned a different reader session.");return {result:v,raw,report_json:`{"family":"STREAM_READER_REPORT","request":${JSON.stringify({route,method,parameters:Object.fromEntries(query)})},"response":${raw}}`,tables:readerTables(v)};
}
export async function createReader(form:ReaderForm,id:string,file:File,signal?:AbortSignal):Promise<ReaderRun> {
  const q=readerCreateQuery(form,id),buffer=await file.arrayBuffer(),digest=await crypto.subtle.digest("SHA-256",buffer);if(signal?.aborted)throw new DOMException("Aborted","AbortError");q.set("source_name",file.name);q.set("source_sha256",Array.from(new Uint8Array(digest),b=>b.toString(16).padStart(2,"0")).join(""));const body=new FormData();body.append("log",file);const run=await request("create",q,"POST",body,signal);if(run.result.kind!==form.kind)throw new Error("The backend returned a different reader kind.");return run;
}
export function getReader(id:string,signal?:AbortSignal):Promise<ReaderRun>{return request("snapshot",new URLSearchParams({session_id:id}),"GET",undefined,signal);}
export function readReader(run:ReaderRun,maximum:string,id:string,signal?:AbortSignal):Promise<ReaderRun>{const n=size(maximum,"Items per read",10000,1);return request("read",new URLSearchParams({session_id:run.result.session_id,expected_revision:String(run.result.revision),maximum_items:String(n),batch_id:id,wait_until_drained:"true"}),"POST",undefined,signal);}
export type ReaderAction="start"|"drain"|"finish"|"register"|"deregister"|"reset_reader"|"release";
export function actReader(run:ReaderRun,action:ReaderAction,signal?:AbortSignal):Promise<ReaderRun>{return request(action==="release"?"release":"action",new URLSearchParams({session_id:run.result.session_id,expected_revision:String(run.result.revision),...(action!=="release"?{action}:{})}),"POST",undefined,signal);}
/** Releases a reader session without waiting (leaving the workspace or replacing the session); a failure, such as a revision another tab moved on, leaves it for the session list. */
export function releaseReaderQuietly(run:ReaderRun):void{if(run.result.released)return;void backendFetch(`/api/stream-reader/release?${new URLSearchParams({session_id:run.result.session_id,expected_revision:String(run.result.revision)})}`,{method:"POST",keepalive:true}).catch(()=>undefined);}
export async function listReaders(signal?:AbortSignal):Promise<ReaderSnapshot[]>{const r=await backendFetch("/api/stream-reader/list",{...(signal?{signal}:{})});if(!r.ok)return error(r);const v:unknown=parseNativeJson(await r.text());if(!record(v)||!Array.isArray(v.sessions)||!v.sessions.every(validReaderSnapshot))throw new Error("The backend returned an invalid reader list.");return v.sessions;}
export async function readerOriginal(id:string):Promise<Blob>{const r=await backendFetch(`/api/stream-reader/source?${new URLSearchParams({session_id:id})}`);if(!r.ok)return error(r);return r.blob();}
export async function readerExport(id:string,scope:"read"|"delivered",format:"json"|"xes",variant="line_by_line",gzip=false):Promise<Blob>{const r=await backendFetch(`/api/stream-reader/export?${new URLSearchParams({session_id:id,scope,format,variant,gzip:String(gzip),preserve_nulls:"true"})}`);if(!r.ok)return error(r);return r.blob();}
export { LIVE_XES_VARIANTS };
