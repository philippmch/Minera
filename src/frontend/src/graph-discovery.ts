import { FOOTPRINTS_DISCOVERY_VARIANTS, FOOTPRINTS_CONFORMANCE_VARIANTS, isMineraExport, type AggregateFootprint, type FootprintsModelVariant, type MineraExport } from "./types";
import type { LogQuerySchema } from "./log-query";
import { nativeCell, nativeJson } from "./native-json";
import { nativeResponseJson, readNativeResponse } from "./native-response";
import { nativeObjectTables, type DiagnosticTable } from "./model-diagnostics";

export type GraphDiscoveryFamily = "footprints" | "transition_system";
export interface FootprintsForm {
  discovery:string[];conformance:string[];model:FootprintsModelVariant;strict:boolean;always:boolean;
  activity:string;caseId:string;timestamp:string;start:string;index:string;overrideIndex:boolean;sort:boolean;marking:string;
}
export interface TransitionForm {view:"sequence"|"set"|"multiset";direction:"forward"|"backward";window:string;includeData:boolean;activity:string;caseId:string;}
export function defaultFootprintsForm():FootprintsForm {return {discovery:['entire_event_log','dfg','process_tree','powl'],conformance:['log_model'],model:'process_tree',strict:false,always:true,activity:'',caseId:'',timestamp:'',start:'',index:'',overrideIndex:false,sort:true,marking:''};}
export function defaultTransitionForm():TransitionForm {return {view:'sequence',direction:'forward',window:'',includeData:false,activity:'',caseId:''};}
export function graphDiscoveryQuery(family:GraphDiscoveryFamily,form:FootprintsForm|TransitionForm,sourceId:string):URLSearchParams {
  if(!sourceId)throw new Error('Refresh the graph discovery source first.');
  const q=new URLSearchParams({family,source_id:sourceId});
  const text=(key:string,value:string)=>{if(value.trim())q.set(key,value.trim());};
  if(family==='footprints'){
    const f=form as FootprintsForm;
    if(!f.discovery.length||!f.conformance.length)throw new Error('Select at least one discovery variant and one conformance variant.');
    if(f.discovery.some(v=>!(FOOTPRINTS_DISCOVERY_VARIANTS as readonly string[]).includes(v))||f.conformance.some(v=>!(FOOTPRINTS_CONFORMANCE_VARIANTS as readonly string[]).includes(v)))throw new Error('Select supported footprint variants.');
    if(f.model==='dfg'&&f.discovery.includes('petri_reach_graph'))throw new Error('Petri reach graph needs a Petri-capable model. Deselect it for a DFG comparison.');
    q.set('footprints_discovery_variants',f.discovery.join(','));q.set('footprints_conformance_variants',f.conformance.join(','));q.set('footprints_model',f.model);
    q.set('footprints_strict',String(f.strict));q.set('footprints_enable_act_always_executed',String(f.always));q.set('footprints_sort_required',String(f.sort));
    for(const[key,value]of [['activity',f.activity],['case_id',f.caseId],['timestamp',f.timestamp],['start_timestamp',f.start]]as const)text('footprints_'+key+'_attribute',value);
    if(f.overrideIndex)q.set('footprints_index_attribute',f.index.trim());
    text('footprints_initial_marking',f.marking);
  }else{
    const f=form as TransitionForm;
    q.set('transition_view',f.view);q.set('transition_direction',f.direction);q.set('transition_include_data',String(f.includeData));
    if(f.window.trim()){
      const value=f.window.trim();
      if(!/^-?\d+$/.test(value)||BigInt(value)<-9223372036854775808n||BigInt(value)>9223372036854775807n)throw new Error('The transition window must be a signed 64-bit integer.');
      q.set('transition_window',value);
    }
    text('transition_activity_attribute',f.activity);text('transition_case_id_attribute',f.caseId);
  }
  return q;
}
export interface GraphDiscovery {
  family:'GRAPH_DISCOVERY';method:GraphDiscoveryFamily;variant:'aggregate'|'view_based';source_id:string;dataset:string;
  source:{case_count:number;event_count:number};analysis_source:{case_count:number;event_count:number;origin:'cached_source'|'cached_alpha_projection'};
  model_origin:'cached_analysis_model'|'selected_source_dfg'|null;model_footprint:AggregateFootprint|null;result:MineraExport;
}
const record=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
export function validGraphDiscovery(value:unknown):value is GraphDiscovery {
  if(!record(value)||value.family!=='GRAPH_DISCOVERY'||!['footprints','transition_system'].includes(String(value.method))||typeof value.source_id!=='string'||!value.source_id||typeof value.dataset!=='string'||!record(value.source)||!record(value.analysis_source)||!isMineraExport(value.result))return false;
  if(!['cached_source','cached_alpha_projection'].includes(String(value.analysis_source.origin))||value.analysis_source.case_count!==value.result.trace_count||value.analysis_source.event_count!==value.result.event_count)return false;
  if(value.method==='footprints')return value.variant==='aggregate'&&!!value.result.footprints&&record(value.model_footprint)&&['cached_analysis_model','selected_source_dfg'].includes(String(value.model_origin));
  return value.variant==='view_based'&&!!value.result.transition_system&&value.model_footprint===null&&value.model_origin===null;
}
export interface GraphDiscoveryRun {value:GraphDiscovery;raw:string;report:string;tables:DiagnosticTable[];}
export function graphDiscoveryTables(value:GraphDiscovery,parameters:URLSearchParams):DiagnosticTable[] {
  const tables=nativeObjectTables(value);
  if(value.result.transition_system){
    const model=value.result.transition_system;
    const rows:unknown[][]=[];
    model.states.forEach(state=>{for(const direction of ['incoming','outgoing']as const)for(const occurrence of state[direction+'_occurrences' as 'incoming_occurrences'|'outgoing_occurrences']??[])rows.push(['state',state.id,direction,occurrence.trace_index,occurrence.event_index]);});
    model.transitions.forEach((transition,index)=>{for(const occurrence of transition.occurrences??[])rows.push(['transition',index,'edge',occurrence.trace_index,occurrence.event_index]);});
    tables.push({id:'traversals',title:'All native traversal references',columns:['kind','native_id','direction','trace_index','event_index'],rows});
  }
  tables.push({id:'configuration',title:'Submitted configuration',columns:['parameter','value'],rows:[...parameters.entries()]});
  return tables;
}
export async function runGraphDiscovery(family:GraphDiscoveryFamily,form:FootprintsForm|TransitionForm,source:LogQuerySchema,signal?:AbortSignal):Promise<GraphDiscoveryRun> {
  const parameters=graphDiscoveryQuery(family,form,source.source_id),path='/api/discover-graph?'+parameters;
  const response=await fetch(path,{method:'POST',signal});const value=await readNativeResponse(response,{path});
  if(!response.ok||record(value)&&'error'in value)throw new Error(record(value)&&typeof value.error==='string'?value.error:'Native graph discovery failed.');
  if(!validGraphDiscovery(value))throw new Error('Native graph discovery returned an invalid result.');
  if(value.method!==family||value.source_id!==source.source_id||value.source.case_count!==source.case_count||value.source.event_count!==source.event_count)throw new Error('Discovery result does not match the active source.');
  if(family==='transition_system'){
    const model=value.result.transition_system!,requested=form as TransitionForm;
    if(model.view!==requested.view||model.direction!==requested.direction||model.include_data!==requested.includeData||requested.window.trim()&&BigInt(String(nativeCell(model,'window')))!==BigInt(requested.window.trim()))throw new Error('Transition system result does not match the requested configuration.');
  }else{
    const requested=form as FootprintsForm,actual=value.result.footprints!;
    const matches=(keys:string[],expected:string[])=>keys.length===new Set(expected).size&&expected.every(key=>keys.includes(key));
    if(actual.conformance?.model!==requested.model||!matches(Object.keys(actual.discovery??{}),requested.discovery)||!matches(Object.keys(actual.conformance??{}).filter(key=>key!=='model'),requested.conformance))throw new Error('Footprint result does not match the requested variants or comparison model.');
  }
  return {value,raw:nativeResponseJson(value),report:nativeJson({family:'GRAPH_DISCOVERY_REPORT',request:{method:'POST',path:'/api/discover-graph',parameters:Object.fromEntries(parameters)},response:value},2),tables:graphDiscoveryTables(value,parameters)};
}
