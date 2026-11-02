import { mineFormEntries, type MinerOptions, type CsvOptions } from "./api";
import { isMineraExport, type MineraExport, type PetriNetModel } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import type { LogQuerySchema } from "./log-query";
import type { OcelPreparedTable } from "./ocel-preparation";
export type WorkflowFamily = "alpha" | "heuristic" | "inductive" | "powl";
export type WorkflowVariant = "classic" | "plus" | "plusplus" | "log" | "imf" | "dfg" | "tree" | "brute_force" | "maximal" | "dynamic_clustering" | "dynamic_clustering_plain";
export const WORKFLOW_VARIANTS:Record<WorkflowFamily,Array<[WorkflowVariant,string]>> = {alpha:[["classic","Classic"],["plus","Alpha+"]],heuristic:[["classic","Classic"],["plusplus","Heuristics++"]],inductive:[["log","IM · log recursion"],["imf","IMf · infrequent"],["dfg","IMd · DFG recursion"]],powl:[["tree","Tree"],["brute_force","Brute force"],["maximal","Maximal"],["dynamic_clustering","Dynamic clustering (frequency)"],["dynamic_clustering_plain","Dynamic clustering (unweighted)"]]};
export interface WorkflowForm {
  variant: WorkflowVariant; activity: string; timestamp: string; start: string; lifecycle: string;
  remove: boolean; maximumRows: string; dependency: string; and: string; frequency: string; minimumActivity: string;
  decoration: "frequency" | "performance"; aggregation: NonNullable<MinerOptions["heuristicPerformanceAggregation"]>;
  businessHours: boolean; slots: string; noise: string; loop: string; once: boolean; paths: string;
  connect: "native" | "true" | "false"; decorationSelector: string; decorationValue: string;
  playoutTraces: string; playoutSteps: string; playoutSeed: string;
  caseId:string;conversion:"standard"|"transition_bordered";disableStrict:boolean;disableFallthroughs:boolean;multiprocessing:boolean;sortTime:boolean;
  filtering:"decreasing_factor"|"dynamic";filteringThreshold:string;orderRatio:string;bruteLimit:string;
}
export function defaultWorkflowForm(variant:WorkflowVariant="classic"):WorkflowForm {return {variant,activity:"",timestamp:"",start:"",lifecycle:"",remove:false,maximumRows:"",dependency:"",and:"",frequency:"",minimumActivity:"",decoration:"frequency",aggregation:"mean",businessHours:false,slots:"",noise:"",loop:"",once:false,paths:"",connect:"native",decorationSelector:"",decorationValue:"",playoutTraces:"",playoutSteps:"",playoutSeed:"",caseId:"",conversion:"standard",disableStrict:false,disableFallthroughs:false,multiprocessing:false,sortTime:false,filtering:"decreasing_factor",filteringThreshold:"",orderRatio:"",bruteLimit:""};}
const numeric=(value:string,name:string,integer=false):number|undefined=>{if(!value.trim())return undefined;const n=Number(value);if(!Number.isFinite(n)||(integer&&(n<0||!Number.isSafeInteger(n))))throw new Error(name+" must be a finite "+(integer?"non-negative integer":"number")+".");return n;};
export function workflowQuery(family:WorkflowFamily,form:WorkflowForm,sourceId:string):URLSearchParams {
  if(!sourceId)throw new Error("Refresh the discovery source first.");
  if(!WORKFLOW_VARIANTS[family].some(([value])=>value===form.variant))throw new Error("Select a valid native discovery variant.");
  if(family==='inductive'||family==='powl'){
    const noise=numeric(form.noise,'Noise threshold');
    if(family==='inductive'&&noise!==undefined&&(noise<0||noise>1))throw new Error('Noise threshold must be in [0,1].');
    const options:MinerOptions=family==='inductive'?{inductiveVariant:form.variant as 'log'|'imf'|'dfg',noiseThreshold:noise,inductiveActivityKey:form.activity.trim()||undefined,inductiveTimestampKey:form.timestamp.trim()||undefined,inductiveCaseIdKey:form.caseId.trim()||undefined,inductiveDisableStrictSequenceCut:form.disableStrict,inductiveDisableFallthroughs:form.disableFallthroughs,inductiveMultiprocessing:form.multiprocessing,inductiveSortEventsByTimestamp:form.sortTime}:{powlVariant:form.variant as NonNullable<MinerOptions['powlVariant']>,powlActivityKey:form.activity.trim()||undefined,powlTimestampKey:form.timestamp.trim()||undefined,powlCaseIdKey:form.caseId.trim()||undefined,powlFilteringType:form.filtering,powlFilteringThreshold:numeric(form.filteringThreshold,'Filtering threshold'),powlOrderFrequencyRatio:numeric(form.orderRatio,'Order frequency ratio'),powlBruteForceActivityLimit:numeric(form.bruteLimit,'Brute-force activity cap',true)};
    const q=new URLSearchParams(mineFormEntries(options));q.set('family',family);q.set('source_id',sourceId);q.set(family+'_variant',form.variant);
    if(family==='inductive')q.set('discovery_tree_variant',form.conversion);
    for(const[field,text]of [['traces',form.playoutTraces],['max_steps',form.playoutSteps],['seed',form.playoutSeed]]as const)if(text.trim()){if(!/^\d+$/.test(text.trim())||BigInt(text.trim())>18446744073709551615n)throw new Error('Playout values must be unsigned 64-bit integers.');q.set('discovery_playout_'+field,text.trim());}
    return q;
  }
  const options:MinerOptions=family==="alpha"?{alphaVariant:form.variant as 'classic'|'plus',alphaActivityAttribute:form.activity.trim()||undefined,alphaRemoveUnconnected:form.remove,alphaMaxRowsToMaterialize:numeric(form.maximumRows,"Input row cap",true)}:{heuristicVariant:form.variant as 'classic'|'plusplus',heuristicActivityAttribute:form.activity.trim()||undefined,heuristicTimestampAttribute:form.timestamp.trim()||undefined,heuristicStartTimestampAttribute:form.start.trim()||undefined,heuristicLifecycleInstanceAttribute:form.lifecycle.trim()||undefined,dependencyThreshold:numeric(form.dependency,"Dependency threshold"),andThreshold:numeric(form.and,"AND threshold"),minEdgeFrequency:numeric(form.frequency,"Minimum edge frequency",true),heuristicMinActivityCount:numeric(form.minimumActivity,"Minimum activity count",true),heuristicDecoration:form.decoration,heuristicPerformanceAggregation:form.aggregation,heuristicBusinessHours:form.businessHours,heuristicBusinessHourSlots:form.slots||undefined,heuristicDfgPreCleaningNoiseThreshold:numeric(form.noise,"Pre-cleaning threshold"),heuristicLoopLengthTwoThreshold:numeric(form.loop,"Length-two-loop threshold"),heuristicKeepOncePerCase:form.once,heuristicMostCommonPaths:form.paths.trim()||undefined};
  const q=new URLSearchParams(mineFormEntries(options));q.set('family',family);q.set('source_id',sourceId);
  if(family==='heuristic'){
    q.set('heuristic_variant',form.variant);
    if(form.connect!=='native')q.set('heuristic_connect_all',form.connect);
    if(form.decorationSelector)q.set('heuristic_plusplus_decoration_selector',form.decorationSelector);
    if(form.decorationValue.trim()){try{JSON.parse(form.decorationValue);}catch{throw new Error('The native decoration value must be valid JSON.');}q.set('heuristic_plusplus_decoration_value',form.decorationValue);}
  }
  for(const [field,label,text] of [['traces','Playout traces',form.playoutTraces],['max_steps','Playout firing cap',form.playoutSteps],['seed','Playout seed',form.playoutSeed]] as const){const n=numeric(text,label,true);if(n!==undefined)q.set('discovery_playout_'+field,String(n));}
  return q;
}
export interface WorkflowDiscovery {
 family:'WORKFLOW_DISCOVERY';method:WorkflowFamily;variant:WorkflowVariant;activity_key:string;source_id:string;dataset:string;source:{case_count:number;event_count:number};
 result:MineraExport;alpha_discovery:Record<string,unknown>|null;pnml:string|null;pnml_error:string|null;
 bpmn?:string|null;bpmn_error?:string|null;ptml?:string|null;ptml_error?:string|null;playout_xes?:string|null;playout_error?:string|null;
 discovery_source?:{case_count:number;event_count:number};source_xes?:string;
 source_xes_options?:CsvOptions;
}
const record=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
export function validWorkflowDiscovery(value:unknown):value is WorkflowDiscovery {
 if(!record(value)||value.family!=='WORKFLOW_DISCOVERY'||!['alpha','heuristic','inductive','powl'].includes(value.method as string)||typeof value.source_id!=='string'||!value.source_id||typeof value.dataset!=='string'||typeof value.activity_key!=='string'||!record(value.source)||!isMineraExport(value.result))return false;
 const counts=record(value.discovery_source)?value.discovery_source:value.source;
 if(counts.case_count!==value.result.trace_count||counts.event_count!==value.result.event_count)return false;
 if(value.method==='alpha'&&(!['classic','plus'].includes(value.variant as string)||!value.result.petri_net))return false;
 if(value.method==='heuristic'&&(!['classic','plusplus'].includes(value.variant as string)||!value.result.heuristic_petri_net||!value.result.heuristics_net||value.result.heuristics_net.variant!==value.variant))return false;
 if((value.method==='inductive'||value.method==='powl')&&(!WORKFLOW_VARIANTS[value.method].some(([variant])=>variant===value.variant)||!record(value.discovery_source)||typeof value.source_xes!=='string'))return false;
 if(value.method==='inductive'||value.method==='powl'){
   const options=value.source_xes_options;
   if(!record(options)||!['xesActivityAttribute','xesCaseAttribute','xesTimestampAttribute'].every(key=>typeof options[key]==='string'&&options[key])||options.xesTimestampSort!==false)return false;
 }
 if(value.method==='inductive'&&(!value.result.process_tree||!value.result.inductive_petri_net))return false;
 if(value.method==='powl'&&(!value.result.powl||!value.result.powl_petri_net))return false;
 if(value.alpha_discovery!==null&&!record(value.alpha_discovery))return false;
 for(const [field,marker]of [['pnml','<pnml'],['bpmn','definitions'],['ptml','<ptml'],['playout_xes','<log']]as const){if(value[field]!==undefined&&value[field]!==null&&(typeof value[field]!=='string'||!(value[field]as string).includes(marker)))return false;}
 return value.pnml!==null||typeof value.pnml_error==='string';
}
const cell=(obj:object,key:string|number):unknown=>{const v=nativeCell(obj,key);return v&&typeof v==='object'?nativeJson(v):v;};
const table=(columns:string[],count:number,rowAt:(i:number)=>unknown[]):OcelPreparedTable=>({columns,count,rowAt});
function properties(value:object):OcelPreparedTable {const keys=Object.keys(value);return table(['field','value'],keys.length,i=>[keys[i],cell(value,keys[i])]);}
function rows(value:unknown[]):OcelPreparedTable {const columns=Array.from(new Set(value.flatMap(row=>record(row)?Object.keys(row):['value'])));return table(['row_index',...columns],value.length,i=>[i,...columns.map(key=>record(value[i])?cell(value[i],key):key==='value'?cell(value,i):undefined)]);}
export function workflowNet(value:WorkflowDiscovery):PetriNetModel {return value.method==='alpha'?value.result.petri_net!:value.method==='heuristic'?value.result.heuristic_petri_net!:value.method==='inductive'?value.result.inductive_petri_net!:value.result.powl_petri_net!;}
export function workflowTables(value:WorkflowDiscovery):Record<string,OcelPreparedTable> {
 const net=workflowNet(value),tables:Record<string,OcelPreparedTable>={source:properties({method:value.method,variant:value.variant,source_id:value.source_id,dataset:value.dataset,activity_key:value.activity_key,...value.source}),model:properties(net),transitions:rows(net.transitions),activities:rows(value.result.activities),formats:properties({pnml_error:value.pnml_error,bpmn_error:value.bpmn_error,ptml_error:value.ptml_error,playout_error:value.playout_error})};
 if(value.alpha_discovery){tables.alpha_summary=properties(value.alpha_discovery);for(const [key,item]of Object.entries(value.alpha_discovery))if(Array.isArray(item))tables['alpha_'+key]=rows(item);}
 if(value.result.heuristics_net){const h=value.result.heuristics_net;tables.heuristic_summary=properties(h);for(const[key,item]of Object.entries(h)){if(Array.isArray(item))tables['heuristic_'+key]=rows(item);else if(record(item))tables['heuristic_'+key]=properties(item);}}
 if(value.discovery_source)tables.discovery_source=properties(value.discovery_source);
 const structure=value.method==='inductive'?value.result.process_tree:value.method==='powl'?value.result.powl:undefined;
 if(structure){const nodes:Array<[string,object]>=[];const visit=(node:object,path:string)=>{nodes.push([path,node]);const children=(node as {children?:object[]}).children;children?.forEach((child,i)=>visit(child,path+'.'+i));};visit(structure,'root');tables.structure_nodes=table(['node_path','native_node_json'],nodes.length,i=>[nodes[i][0],nativeJson(nodes[i][1])]);}
 return tables;
}
export interface WorkflowRun {value:WorkflowDiscovery;raw:string;report:string;tables:Record<string,OcelPreparedTable>;
 /** The activity selector the user typed for this run, or undefined when discovery used the native default. */
 replayActivityKey?:string;}
/** Checking a discovered model sends a replay selector only when the run used an explicit one: the native
 * default (reported as concept:name) is not an attribute of CSV-sourced logs, so naming it fails the check. */
export function workflowReplayKey(run:WorkflowRun):string|undefined {return run.replayActivityKey===undefined?undefined:run.value.activity_key;}
export async function runWorkflowDiscovery(family:WorkflowFamily,form:WorkflowForm,schema:LogQuerySchema,signal?:AbortSignal):Promise<WorkflowRun> {
 const q=workflowQuery(family,form,schema.source_id),response=await fetch('/api/discover-workflow?'+q,{method:'POST',...(signal?{signal}:{})}),raw=await response.text();let value:unknown;
 try{value=parseNativeJson(raw);}catch{throw new Error('The backend returned invalid discovery JSON.');}
 if(record(value)&&'error'in value)throw new Error(String(value.error));if(!response.ok)throw new Error('Workflow discovery returned '+response.status+'.');if(!validWorkflowDiscovery(value))throw new Error('The backend returned an invalid native workflow discovery result.');
 if(value.method!==family||value.variant!==form.variant||value.source_id!==schema.source_id||value.source.case_count!==schema.case_count||value.source.event_count!==schema.event_count)throw new Error('Discovery result does not match the selected source and variant.');
 const tables=workflowTables(value),entries=Array.from(q);tables.configuration=table(['parameter','value'],entries.length,i=>entries[i]);return{value,raw,tables,...(form.activity.trim()?{replayActivityKey:form.activity.trim()}:{}),report:'{"family":"WORKFLOW_DISCOVERY_REPORT","request":'+JSON.stringify({parameters:Object.fromEntries(q)})+',"response":'+raw+'}'};
}
export function workflowModelFile(run:WorkflowRun,format:'pnml'|'bpmn'|'ptml'):File {const raw=run.value[format];if(!raw)throw new Error(run.value[(format+'_error')as 'pnml_error']??'This discovery result has no '+format+' export.');return new File([raw],`minera-${run.value.method}-${run.value.variant}.${format}`,{type:'application/xml'});}
