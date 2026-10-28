import { isMineraExport,type MineraExport } from "./types";
import { nativeJson } from "./native-json";
import { nativeResponseJson,readNativeResponse } from "./native-response";
import { nativeObjectTables,type DiagnosticTable } from "./model-diagnostics";
import type { LogQuerySchema } from "./log-query";
export type TimingFamily='spectrum'|'batches';
export const TIMING_VARIANTS={spectrum:['log','dataframe','lazyframe','log_disconnected','dataframe_disconnected','lazyframe_disconnected','connected','disconnected'],batches:['log','pandas','polars']} as const;
export interface TimingForm {variant:string;pattern:string;sample:string;sort:boolean;activity:string;timestamp:string;caseId:string;resource:string;start:string;eventId:string;merge:string;minimum:string;overrideStart:boolean;}
export function defaultTimingForm(family:TimingFamily):TimingForm {return {variant:TIMING_VARIANTS[family][0],pattern:'',sample:'',sort:true,activity:'',timestamp:'',caseId:'',resource:'',start:'',eventId:'',merge:'',minimum:'',overrideStart:false};}
export function timingQuery(family:TimingFamily,form:TimingForm,sourceId:string):URLSearchParams {
  if(!sourceId)throw new Error('Refresh the timing source first.');
  if(!(TIMING_VARIANTS[family]as readonly string[]).includes(form.variant))throw new Error('Select a supported native timing variant.');
  const q=new URLSearchParams({family,source_id:sourceId});q.set(family==='spectrum'?'spectrum_variant':'batch_variant',form.variant);
  const text=(key:string,value:string)=>{if(value.trim())q.set(key,value.trim());};
  const integer=(key:string,value:string,minimum=0n)=>{if(!value.trim())return;if(!/^\d+$/.test(value.trim())||BigInt(value.trim())<minimum||BigInt(value.trim())>18446744073709551615n)throw new Error('Counts must be unsigned 64-bit integers within the native range.');q.set(key,value.trim());};
  if(family==='spectrum'){
    const pattern=form.pattern.split(',').map(value=>value.trim());if(pattern.length<2||pattern.some(value=>!value))throw new Error('Enter at least two comma-separated pattern activities.');
    q.set('spectrum_activities',pattern.join(','));integer('spectrum_sample_size',form.sample);q.set('spectrum_sort_log',String(form.sort));
    text('spectrum_activity_key',form.activity);text('spectrum_timestamp_key',form.timestamp);text('spectrum_case_id_key',form.caseId);
  }else{
    if(form.eventId.trim()&&form.variant==='log')throw new Error('Event identity requires the pandas or polars batch adapter.');
    text('batch_activity_attribute',form.activity);text('batch_timestamp_attribute',form.timestamp);text('batch_case_id_attribute',form.caseId);text('batch_resource_attribute',form.resource);text('batch_event_id_attribute',form.eventId);
    if(form.overrideStart)q.set('batch_start_timestamp_attribute',form.start.trim());
    integer('batch_min_size',form.minimum);
    if(form.merge.trim()){const n=Number(form.merge);if(!Number.isFinite(n))throw new Error('Batch merge distance must be a finite number.');q.set('batch_merge_distance_seconds',form.merge.trim());}
  }
  return q;
}
export interface TimingDiscovery {family:'TIMING_DISCOVERY';method:TimingFamily;variant:string;source_id:string;dataset:string;source:{case_count:number;event_count:number};result:MineraExport;}
const record=(v:unknown):v is Record<string,unknown>=>!!v&&typeof v==='object'&&!Array.isArray(v);
export function validTimingDiscovery(v:unknown):v is TimingDiscovery {
  if(!record(v)||v.family!=='TIMING_DISCOVERY'||!['spectrum','batches'].includes(String(v.method))||typeof v.source_id!=='string'||!v.source_id||typeof v.dataset!=='string'||!record(v.source)||!isMineraExport(v.result))return false;
  if(v.source.case_count!==v.result.trace_count||v.source.event_count!==v.result.event_count||!(TIMING_VARIANTS[v.method as TimingFamily]as readonly string[]).includes(String(v.variant)))return false;
  return v.method==='spectrum'?!!v.result.performance_spectrum:!!v.result.batch_discovery;
}
export interface TimingRun {value:TimingDiscovery;raw:string;report:string;tables:DiagnosticTable[];}
export async function runTimingDiscovery(family:TimingFamily,form:TimingForm,source:LogQuerySchema,signal?:AbortSignal):Promise<TimingRun> {
  const q=timingQuery(family,form,source.source_id),path='/api/discover-timing?'+q;const response=await fetch(path,{method:'POST',signal});const value=await readNativeResponse(response,{path});
  if(!response.ok||record(value)&&'error'in value)throw new Error(record(value)&&typeof value.error==='string'?value.error:'Native timing discovery failed.');
  if(!validTimingDiscovery(value))throw new Error('Native timing discovery returned an invalid result.');
  if(value.method!==family||value.variant!==form.variant||value.source_id!==source.source_id||value.source.case_count!==source.case_count||value.source.event_count!==source.event_count)throw new Error('Timing result does not match the requested source or variant.');
  const tables=nativeObjectTables(value);tables.push({id:'configuration',title:'Submitted configuration',columns:['parameter','value'],rows:[...q.entries()]});
  return {value,raw:nativeResponseJson(value),report:nativeJson({family:'TIMING_DISCOVERY_REPORT',request:{method:'POST',path:'/api/discover-timing',parameters:Object.fromEntries(q)},response:value},2),tables};
}
