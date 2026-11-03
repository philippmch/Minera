import {parseDiscoveryStringVector, discoveryStringVectorJson, type DiscoveryVectorMode} from "./discovery-string-vectors";
import { backendFetch } from "./backend-fetch";
import { isMineraExport,type MineraExport } from "./types";
import { nativeCell,nativeJson } from "./native-json";
import { nativeResponseJson,readNativeResponse } from "./native-response";
import { nativeObjectTables,type DiagnosticTable } from "./model-diagnostics";
import type { LogQuerySchema } from "./log-query";
export const DFG_STATISTICAL=['native','frequency','frequency_greedy','frequency_triples','performance','performance_greedy','case_attributes','clean','clean_polars','clean_time'] as const;
export const DFG_CAUSAL=['alpha','heuristic'] as const;
export const DFG_AGGREGATIONS=['mean','median','min','max','stdev','sum','raw_values','all'] as const;
export type DfgNativeSwitch='native'|'true'|'false';
export interface DfgDiscoveryForm {statistical:string[];causal:string[];window:string;keep:DfgNativeSwitch;activity:string;timestamp:string;start:string;overrideStart:boolean;aggregation:string;business:DfgNativeSwitch;overrideSlots:boolean;slots:string;overrideAttributes:boolean;attributes:string;attributesMode:DiscoveryVectorMode;attributesJson:string;nodes:DfgNativeSwitch;}
export const defaultDfgDiscoveryForm=():DfgDiscoveryForm=>({statistical:['frequency'],causal:[],window:'',keep:'native',activity:'',timestamp:'',start:'',overrideStart:false,aggregation:'native',business:'native',overrideSlots:false,slots:'',overrideAttributes:false,attributes:'',attributesMode:'legacy',attributesJson:'["concept:name"]',nodes:'native'});
const selected=(catalog:readonly string[],choices:string[])=>{if(choices.some(c=>!catalog.includes(c)))throw new Error('Select supported native DFG variants.');return catalog.filter(c=>choices.includes(c));};
export function dfgDiscoveryQuery(form:DfgDiscoveryForm,sourceId:string):URLSearchParams {
  if(!sourceId)throw new Error('Refresh the DFG discovery source first.');
  const statistical=selected(DFG_STATISTICAL,form.statistical),causal=selected(DFG_CAUSAL,form.causal);if(!statistical.length&&!causal.length)throw new Error('Select at least one statistical or causal DFG variant.');
  const q=new URLSearchParams({source_id:sourceId});if(statistical.length)q.set('dfg_statistical_variants',statistical.join(','));if(causal.length)q.set('dfg_causal_variants',causal.join(','));
  if(form.window.trim()){const value=form.window.trim();if(!/^\d+$/.test(value)||BigInt(value)>18446744073709551615n)throw new Error('DFG window must be an unsigned 64-bit integer.');q.set('dfg_window',value);}
  const text=(key:string,value:string)=>{if(value.trim())q.set(key,value.trim());};const flag=(key:string,value:DfgNativeSwitch)=>{if(!['native','true','false'].includes(value))throw new Error('Select a native boolean setting.');if(value!=='native')q.set(key,value);};
  text('dfg_activity_attribute',form.activity);text('dfg_timestamp_attribute',form.timestamp);if(form.overrideStart)q.set('dfg_start_timestamp_attribute',form.start.trim());else text('dfg_start_timestamp_attribute',form.start);
  if(form.aggregation!=='native'){if(!(DFG_AGGREGATIONS as readonly string[]).includes(form.aggregation))throw new Error('Select a native performance aggregation.');q.set('dfg_performance_aggregation',form.aggregation);}
  flag('dfg_keep_once_per_case',form.keep);flag('dfg_business_hours',form.business);flag('dfg_return_node_attributes',form.nodes);if(form.overrideSlots)q.set('dfg_business_hour_slots',form.slots.trim());if(form.overrideAttributes){if(form.attributesMode==='literal_json')q.set('dfg_case_attributes_json',discoveryStringVectorJson(parseDiscoveryStringVector(form.attributesJson,'DFG case attributes'),'DFG case attributes'));else q.set('dfg_case_attributes',form.attributes.trim());}return q;
}
export interface DfgDiscovery {family:'DFG_DISCOVERY';source_id:string;dataset:string;source:{case_count:number;event_count:number};input_origin:'cached_source_log';statistical_variants:string[];causal_variants:string[];result:MineraExport;}
const record=(v:unknown):v is Record<string,unknown>=>!!v&&typeof v==='object'&&!Array.isArray(v);
const equal=(a:readonly string[],b:readonly string[])=>a.length===b.length&&a.every((v,i)=>v===b[i]);
export function validDfgDiscovery(v:unknown):v is DfgDiscovery {
  if(!record(v)||v.family!=='DFG_DISCOVERY'||typeof v.source_id!=='string'||!v.source_id||typeof v.dataset!=='string'||v.input_origin!=='cached_source_log'||!record(v.source)||!isMineraExport(v.result)||!v.result.dfg_variants||!Array.isArray(v.statistical_variants)||!Array.isArray(v.causal_variants))return false;
  if(v.source.case_count!==v.result.trace_count||v.source.event_count!==v.result.event_count||!v.statistical_variants.length&&!v.causal_variants.length)return false;
  const stats=v.statistical_variants,causal=v.causal_variants;
  return stats.every((s):s is string=>typeof s==='string'&&(DFG_STATISTICAL as readonly string[]).includes(s))&&causal.every((s):s is string=>typeof s==='string'&&(DFG_CAUSAL as readonly string[]).includes(s))&&new Set(stats).size===stats.length&&new Set(causal).size===causal.length&&equal(stats,v.result.dfg_variants.statistical.map(s=>s.variant))&&equal(causal,v.result.dfg_variants.causal.map(s=>s.variant));
}
export function dfgDiscoveryTables(value:DfgDiscovery,q:URLSearchParams):DiagnosticTable[] {
  const tables=nativeObjectTables(value),label=(id:number)=>value.result.activities[id]?.name??String(id),columns=['source_id','source_label','target_id','target_label','count'];
  for(const analysis of value.result.dfg_variants!.statistical){
    const prefix='statistical/'+analysis.variant,edges=analysis.frequency_dfg??analysis.clean?.dfg;
    if(edges)tables.push({id:prefix+'/edges',title:analysis.variant+' complete frequency edges',columns,rows:edges.map(e=>[nativeCell(e,'source'),label(e.source),nativeCell(e,'target'),label(e.target),nativeCell(e,'count')]),records:edges});
    if(analysis.frequency_triples)tables.push({id:prefix+'/triples',title:'Complete frequency triples',columns:['first_id','first_label','middle_id','middle_label','last_id','last_label','count'],rows:analysis.frequency_triples.map(e=>[nativeCell(e,'first'),label(e.first),nativeCell(e,'middle'),label(e.middle),nativeCell(e,'last'),label(e.last),nativeCell(e,'count')]),records:analysis.frequency_triples});
    if(analysis.performance_dfg){
      tables.push({id:prefix+'/performance',title:analysis.variant+' complete performance edges',columns:['source_id','source_label','target_id','target_label','count','mean','median','min','max','stdev','sum','raw_values','value_type','value'],rows:analysis.performance_dfg.map(e=>[nativeCell(e,'source'),label(e.source),nativeCell(e,'target'),label(e.target),...['count','mean','median','min','max','stdev','sum'].map(k=>nativeCell(e.summary,k)),e.raw_values,Array.isArray(e.value)?'array':typeof e.value,nativeCell(e,'value')]),records:analysis.performance_dfg});
      tables.push({id:prefix+'/raw_samples',title:analysis.variant+' every native performance sample',columns:['source_id','source_label','target_id','target_label','sample_index','seconds'],rows:analysis.performance_dfg.flatMap(e=>e.raw_values.map((_,i)=>[nativeCell(e,'source'),label(e.source),nativeCell(e,'target'),label(e.target),i,nativeCell(e.raw_values,i)]))});
    }
    if(analysis.clean)tables.push({id:prefix+'/boundaries',title:analysis.variant+' complete start/end activities',columns:['kind','activity_id','activity_label','count'],rows:([['start',analysis.clean.start_activities],['end',analysis.clean.end_activities]]as const).flatMap(([kind,items])=>items.map(e=>[kind,nativeCell(e,'activity'),label(e.activity),nativeCell(e,'count')]))});
    if(analysis.clean_time)tables.push({id:prefix+'/times',title:'Complete clean activity times',columns:['activity_id','activity_label','mean_seconds_from_case_start'],rows:analysis.clean_time.map(e=>[nativeCell(e,'activity'),label(e.activity),nativeCell(e,'mean_seconds_from_case_start')]),records:analysis.clean_time});
    if(analysis.case_attributes){
      const rows:unknown[][]=[];for(const e of analysis.case_attributes.edges)for(const a of e.attributes)for(const v of a.values)rows.push(['edge',nativeCell(e,'source'),label(e.source),nativeCell(e,'target'),label(e.target),a.attribute,v.value,nativeCell(v,'count')]);for(const e of analysis.case_attributes.nodes)for(const a of e.attributes)for(const v of a.values)rows.push(['node',nativeCell(e,'activity'),label(e.activity),'','',a.attribute,v.value,nativeCell(v,'count')]);
      tables.push({id:prefix+'/attributes',title:'Every native edge/node attribute value',columns:['kind','source_id','source_label','target_id','target_label','attribute','value','count'],rows});
    }
  }
  for(const a of value.result.dfg_variants!.causal)tables.push({id:'causal/'+a.variant+'/relations',title:a.variant+' complete causal relations',columns:['source_id','source_label','target_id','target_label','value'],rows:a.relations.map(e=>[nativeCell(e,'source'),label(e.source),nativeCell(e,'target'),label(e.target),nativeCell(e,'value')]),records:a.relations});
  tables.push({id:'configuration',title:'Submitted configuration',columns:['parameter','value'],rows:[...q.entries()]});
  for(const table of tables)if(!table.records)table.records=table.rows.map(row=>Object.fromEntries(table.columns.map((column,index)=>[column,row[index]])));
  return tables;
}
export interface DfgDiscoveryRun {value:DfgDiscovery;raw:string;report:string;tables:DiagnosticTable[];}
export async function runDfgDiscovery(form:DfgDiscoveryForm,source:LogQuerySchema,signal?:AbortSignal):Promise<DfgDiscoveryRun> {
  const q=dfgDiscoveryQuery(form,source.source_id),path='/api/discover-dfg?'+q;const response=await backendFetch(path,{method:'POST',signal});const value=await readNativeResponse(response,{path});if(!response.ok||record(value)&&'error'in value)throw new Error(record(value)&&typeof value.error==='string'?value.error:'Native DFG discovery failed.');if(!validDfgDiscovery(value))throw new Error('Native DFG discovery returned an invalid result.');
  if(value.source_id!==source.source_id||value.source.case_count!==source.case_count||value.source.event_count!==source.event_count||!equal(value.statistical_variants,selected(DFG_STATISTICAL,form.statistical))||!equal(value.causal_variants,selected(DFG_CAUSAL,form.causal)))throw new Error('DFG result does not match the requested source or variants.');
  return {value,raw:nativeResponseJson(value),report:nativeJson({family:'DFG_DISCOVERY_REPORT',request:{method:'POST',path:'/api/discover-dfg',parameters:Object.fromEntries(q)},response:value},2),tables:dfgDiscoveryTables(value,q)};
}
