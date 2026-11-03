import { isMineraExport,isTemporalConformanceResult,type MineraExport,type TemporalConformanceResult } from "./types";
import { backendFetch } from "./backend-fetch";
import { nativeCell,nativeJson,parseNativeJson } from "./native-json";
import { nativeResponseJson,readNativeResponse } from "./native-response";
import { nativeObjectTables,parseDiagnosticWeight,type DiagnosticTable } from "./model-diagnostics";
import type { LogQuerySchema } from "./log-query";

export type BehaviorFamily='temporal_profile'|'temporal_conformance'|'minimum_self_distance'|'correlation';
export const BEHAVIOR_ADAPTERS=['log','stream','dataframe','lazyframe'] as const;
export const BEHAVIOR_VARIANTS={temporal_profile:BEHAVIOR_ADAPTERS,temporal_conformance:BEHAVIOR_ADAPTERS,minimum_self_distance:['log','pandas','polars'],correlation:['classic','classic_split','trace_based']} as const;
export type NativeSwitch='native'|'true'|'false';
export interface BehaviorForm {variant:string;adapter:string;activity:string;caseId:string;completion:string;start:string;sample:string;exact:NativeSwitch;strict:NativeSwitch;business:NativeSwitch;overrideSlots:boolean;slots:string;overrideStart:boolean;zeta:string;profile:string;}
export function defaultBehaviorForm(family:BehaviorFamily):BehaviorForm {return {variant:BEHAVIOR_VARIANTS[family][0],adapter:'dataframe',activity:'',caseId:'',completion:'',start:'',sample:'',exact:'native',strict:'native',business:'native',overrideSlots:false,slots:'',overrideStart:false,zeta:'',profile:'[]'};}
const record=(v:unknown):v is Record<string,unknown>=>!!v&&typeof v==='object'&&!Array.isArray(v);
export const validProfile=(v:unknown):v is Array<[string,string,number,number]>=>Array.isArray(v)&&v.every(e=>Array.isArray(e)&&e.length===4&&typeof e[0]==='string'&&typeof e[1]==='string'&&typeof e[2]==='number'&&Number.isFinite(e[2])&&e[2]>=0&&typeof e[3]==='number'&&Number.isFinite(e[3])&&e[3]>=0);
export function behaviorQuery(family:BehaviorFamily,form:BehaviorForm,sourceId:string):URLSearchParams {
  if(!sourceId)throw new Error('Refresh the behavior source first.');
  if(!(BEHAVIOR_VARIANTS[family]as readonly string[]).includes(form.variant))throw new Error('Select a supported native behavior variant.');
  const q=new URLSearchParams({family,source_id:sourceId,variant:form.variant});
  const text=(key:string,value:string)=>{if(value.trim())q.set(key,value.trim());};
  const flag=(key:string,value:NativeSwitch)=>{if(!['native','true','false'].includes(value))throw new Error('Select a native boolean setting.');if(value!=='native')q.set(key,value);};
  if(family.startsWith('temporal')){
    text('temporal_activity_attribute',form.activity);text('temporal_case_id_attribute',form.caseId);text('temporal_source_timestamp_attribute',form.completion);text('temporal_target_timestamp_attribute',form.start);
    flag('temporal_business_hours',form.business);flag('temporal_strict_attributes',form.strict);if(form.overrideSlots)q.set('temporal_business_hour_slots',form.slots.trim());
    if(family==='temporal_conformance'){
      let value:unknown;try{value=parseNativeJson(form.profile);}catch{throw new Error('Profile JSON must contain source, target, mean and standard-deviation tuples.');}
      if(!validProfile(value))throw new Error('Profile JSON must contain source, target and finite non-negative mean/standard-deviation tuples.');
      q.set('temporal_profile_json',form.profile);
      if(form.zeta.trim()){const value=parseDiagnosticWeight(form.zeta);if(value===undefined||Number.isNaN(value))throw new Error('Zeta must be a number other than NaN.');q.set('temporal_zeta',form.zeta.trim());}
    }
  }else if(family==='minimum_self_distance'){
    text('minimum_self_distance_activity_key',form.activity);text('minimum_self_distance_case_id_key',form.caseId);
  }else{
    if(!(BEHAVIOR_ADAPTERS as readonly string[]).includes(form.adapter))throw new Error('Select a supported correlation input adapter.');q.set('adapter',form.adapter);
    text('correlation_activity_attribute',form.activity);text('correlation_case_id_attribute',form.caseId);text('correlation_timestamp_attribute',form.completion);
    if(form.overrideStart)q.set('correlation_start_timestamp_attribute',form.start.trim());else text('correlation_start_timestamp_attribute',form.start);
    flag('correlation_exact_time_matching',form.exact);
    if(form.sample.trim()){const value=form.sample.trim();if(!/^\d+$/.test(value)||BigInt(value)<1n||BigInt(value)>18446744073709551615n)throw new Error('Correlation sample size must be a positive unsigned 64-bit integer.');q.set('correlation_sample_size',value);}
  }
  return q;
}
export interface BehaviorAnalysis {family:'BEHAVIOR_ANALYSIS';method:BehaviorFamily;variant:string;adapter:string;source_id:string;dataset:string;source:{case_count:number;event_count:number};input_origin:string;result:MineraExport;profile?:Array<[string,string,number,number]>;conformance?:TemporalConformanceResult;}
export function validBehaviorAnalysis(v:unknown):v is BehaviorAnalysis {
  if(!record(v)||v.family!=='BEHAVIOR_ANALYSIS'||!['temporal_profile','temporal_conformance','minimum_self_distance','correlation'].includes(String(v.method))||typeof v.source_id!=='string'||!v.source_id||typeof v.dataset!=='string'||!record(v.source)||typeof v.input_origin!=='string'||!isMineraExport(v.result))return false;
  if(v.source.case_count!==v.result.trace_count||v.source.event_count!==v.result.event_count||!(BEHAVIOR_VARIANTS[v.method as BehaviorFamily]as readonly string[]).includes(String(v.variant))||!(BEHAVIOR_ADAPTERS as readonly string[]).includes(String(v.adapter)))return false;
  if(String(v.method).startsWith('temporal'))return validProfile(v.profile)&&Array.isArray(v.result.temporal_profile)&&(v.method!=='temporal_conformance'||isTemporalConformanceResult(v.conformance));
  return v.method==='minimum_self_distance'?Array.isArray(v.result.minimum_self_distances):!!v.result.correlation_mining&&v.result.correlation_mining.variants.length===1&&v.result.correlation_mining.variants[0].variant===v.variant;
}
export function behaviorTables(value:BehaviorAnalysis,parameters:URLSearchParams):DiagnosticTable[] {
  const tables=nativeObjectTables(value),label=(id:unknown)=>id===null?null:value.result.activities[Number(id)]?.name??String(id);
  if(value.result.temporal_profile)tables.push({id:'relations',title:'Temporal moments with activity labels',columns:['source_id','source_label','target_id','target_label','count','mean_seconds','standard_deviation_seconds'],rows:value.result.temporal_profile.map(e=>[nativeCell(e,'source'),label(e.source),nativeCell(e,'target'),label(e.target),nativeCell(e,'count'),nativeCell(e,'mean_seconds'),nativeCell(e,'standard_deviation_seconds')]),records:value.result.temporal_profile});
  if(value.result.minimum_self_distances)tables.push({id:'relations',title:'Minimum self-distance with activity labels',columns:['activity_id','activity_label','distance','witness_ids','witness_labels'],rows:value.result.minimum_self_distances.map(e=>[nativeCell(e,'activity'),label(e.activity),nativeCell(e,'distance'),e.witnesses,e.witnesses.map(label)]),records:value.result.minimum_self_distances});
  if(value.conformance){
    tables.push({id:'temporal_cases',title:'Every native conformance case bucket',columns:['case_index','deviation_count','deviations'],rows:value.conformance.traces.map((trace,i)=>[i,trace.length,trace]),records:value.conformance.traces});
    tables.push({id:'temporal_deviations',title:'Every temporal deviation',columns:['case_index','deviation_index','case_id','source','target','elapsed_seconds','standard_deviation_distance'],rows:value.conformance.traces.flatMap((trace,i)=>trace.map((d,j)=>[i,j,...['case_id','source','target','elapsed_seconds','standard_deviation_distance'].map(key=>nativeCell(d,key))])),records:value.conformance.traces.flat()});
  }
  tables.push({id:'configuration',title:'Submitted configuration',columns:['parameter','value'],rows:[...parameters.entries()]});return tables;
}
export interface BehaviorRun {value:BehaviorAnalysis;raw:string;report:string;tables:DiagnosticTable[];form:BehaviorForm;}
export async function runBehaviorAnalysis(family:BehaviorFamily,form:BehaviorForm,source:LogQuerySchema,signal?:AbortSignal):Promise<BehaviorRun> {
  const q=behaviorQuery(family,form,source.source_id),path='/api/analyze-behavior?'+q;const response=await backendFetch(path,{method:'POST',signal});const value=await readNativeResponse(response,{path});
  if(!response.ok||record(value)&&'error'in value)throw new Error(record(value)&&typeof value.error==='string'?value.error:'Native behavior analysis failed.');
  if(!validBehaviorAnalysis(value))throw new Error('Native behavior analysis returned an invalid result.');
  const adapter=family==='correlation'?form.adapter:family==='minimum_self_distance'?({log:'log',pandas:'dataframe',polars:'lazyframe'}as Record<string,string>)[form.variant]:form.variant;
  if(value.method!==family||value.variant!==form.variant||value.adapter!==adapter||value.source_id!==source.source_id||value.source.case_count!==source.case_count||value.source.event_count!==source.event_count)throw new Error('Behavior result does not match the requested source, adapter or variant.');
  return {value,raw:nativeResponseJson(value),report:nativeJson({family:'BEHAVIOR_ANALYSIS_REPORT',request:{method:'POST',path:'/api/analyze-behavior',parameters:Object.fromEntries(q)},response:value},2),tables:behaviorTables(value,q),form:{...form}};
}
