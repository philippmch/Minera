import { DECLARE_TEMPLATES, type MineraExport } from "./types";
import { nativeCell } from "./native-json";
import { type OcelPreparedTable } from "./ocel-preparation";
export { DECLARE_TEMPLATES };
export interface DeclareStreamRule { constraint_index:number;template:string;first:string;second:string;binary:boolean;support:number;confidence:number; }
export interface DeclareStreamModel { family:"DECLARE_STREAM_MODEL";rule_count:number;rules:DeclareStreamRule[]; }
export interface DeclareStreamIdentity { constraint_index:number;template:string;first:string;second:string;binary:boolean; }
export interface DeclareStreamState extends DeclareStreamIdentity {state:string;}
export interface DeclareStreamEvent {case_id:string;activity:string;timestamp:string;event_index:number;deviations:number;violated_constraints:DeclareStreamIdentity[];}
export interface DeclareStreamResult {family:"STREAMING_DECLARE";total_events_processed:number;total_deviations:number;model:DeclareStreamModel;deviations_per_time:{timestamp:string;deviations:number}[];cases:Record<string,{events:number;deviations:number;constraint_states:DeclareStreamState[]}>;event_results?:DeclareStreamEvent[];}
const record=(v:unknown):v is Record<string,unknown>=>!!v&&typeof v==="object"&&!Array.isArray(v);
const count=(v:unknown):v is number=>Number.isSafeInteger(v)&&(v as number)>=0;
const int=(v:unknown)=>Number.isInteger(v)&&(v as number)>=-2147483648&&(v as number)<=2147483647;
export function declareBinary(template:string):boolean {return !["existence","exactly_one","init","absence"].includes(template);}
function identity(v:unknown,model:DeclareStreamModel):v is DeclareStreamIdentity {
  if(!record(v)||!count(v.constraint_index)||v.constraint_index>=model.rules.length)return false;const rule=model.rules[v.constraint_index];return ["template","first","second","binary"].every(key=>v[key]===(rule as unknown as Record<string,unknown>)[key]);
}
export function validDeclareStreamModel(v:unknown):v is DeclareStreamModel {
  return record(v)&&v.family==="DECLARE_STREAM_MODEL"&&count(v.rule_count)&&Array.isArray(v.rules)&&v.rule_count===v.rules.length&&v.rules.every((r,i)=>record(r)&&r.constraint_index===i&&typeof r.template==="string"&&typeof r.first==="string"&&typeof r.second==="string"&&typeof r.binary==="boolean"&&int(r.support)&&int(r.confidence)&&(!DECLARE_TEMPLATES.includes(r.template as typeof DECLARE_TEMPLATES[number])||r.binary===declareBinary(r.template)));
}
export function validDeclareStreamEvent(v:unknown,model:DeclareStreamModel):v is DeclareStreamEvent {
  return record(v)&&typeof v.case_id==="string"&&typeof v.activity==="string"&&typeof v.timestamp==="string"&&count(v.event_index)&&v.event_index>0&&count(v.deviations)&&Array.isArray(v.violated_constraints)&&v.deviations===v.violated_constraints.length&&v.violated_constraints.every(rule=>identity(rule,model))&&new Set(v.violated_constraints.map(r=>(r as DeclareStreamIdentity).constraint_index)).size===v.violated_constraints.length;
}
export function isDeclareStreamResult(v:unknown):v is DeclareStreamResult {
  if(!record(v)||v.family!=="STREAMING_DECLARE"||!count(v.total_events_processed)||!count(v.total_deviations)||!validDeclareStreamModel(v.model)||!Array.isArray(v.deviations_per_time)||!record(v.cases))return false;
  const model=v.model;
  if(v.deviations_per_time.length!==v.total_events_processed||!v.deviations_per_time.every(t=>record(t)&&typeof t.timestamp==="string"&&count(t.deviations))||v.deviations_per_time.reduce((n,t)=>n+(t as {deviations:number}).deviations,0)!==v.total_deviations)return false;
  if(!Object.values(v.cases).every(c=>record(c)&&count(c.events)&&count(c.deviations)&&Array.isArray(c.constraint_states)&&c.constraint_states.length===model.rule_count&&c.constraint_states.every((s,i)=>identity(s,model)&&s.constraint_index===i&&typeof (s as DeclareStreamState).state==="string")))return false;
  return v.event_results===undefined||Array.isArray(v.event_results)&&v.event_results.length===v.total_events_processed&&v.event_results.every((e,i)=>validDeclareStreamEvent(e,model)&&e.event_index===i+1);
}
export function defaultDeclareRules():string {return JSON.stringify({rules:[{template:"response",first:"A",second:"B",binary:true},{template:"chainresponse",first:"A",second:"B",binary:true},{template:"absence",first:"X",binary:false}]},null,2);}
export function declareRulesFile(raw:string):File {let value:unknown;try{value=JSON.parse(raw);}catch{throw new Error("Declare rules must be valid JSON.");}if(!record(value)||!Array.isArray(value.rules))throw new Error("Declare rules JSON requires a rules array.");return new File([raw],"minera-declare-rules.json",{type:"application/json"});}
export function displayedDeclareFile(data:MineraExport):File {if(!data.declare)throw new Error("The displayed analysis has no Declare model.");return new File([JSON.stringify({activities:data.activities.map(a=>a.name),declare:data.declare,provenance:{dataset:data.dataset}},null,2)],"minera-displayed-declare-model.json",{type:"application/json"});}
export function declareStreamTables(result:DeclareStreamResult):Record<string,OcelPreparedTable> {
  const ruleColumns=["constraint_index","template","first","second","binary","support","confidence"];
  const tables:Record<string,OcelPreparedTable>={declare_rules:{columns:ruleColumns,count:result.model.rules.length,rowAt:i=>ruleColumns.map(key=>nativeCell(result.model.rules[i],key))},time_deviations:{columns:["entry_position","timestamp","deviations"],count:result.deviations_per_time.length,rowAt:i=>[i,result.deviations_per_time[i].timestamp,nativeCell(result.deviations_per_time[i],"deviations")]}};
  const states=Object.entries(result.cases).flatMap(([id,c])=>c.constraint_states.map(state=>({id,state})));const stateColumns=["constraint_index","template","first","second","binary","state"];
  tables.constraint_states={columns:["case_id",...stateColumns],count:states.length,rowAt:i=>[states[i].id,...stateColumns.map(key=>nativeCell(states[i].state,key))]};
  if(result.event_results){const columns=["case_id","activity","timestamp","event_index","deviations"];tables.declare_events={columns,count:result.event_results.length,rowAt:i=>columns.map(key=>nativeCell(result.event_results![i],key))};const violations=result.event_results.flatMap(e=>e.violated_constraints.map(rule=>({event:e,rule})));const names=["constraint_index","template","first","second","binary"];tables.event_violations={columns:["event_index","case_id","timestamp",...names],count:violations.length,rowAt:i=>{const v=violations[i];return [nativeCell(v.event,"event_index"),v.event.case_id,v.event.timestamp,...names.map(key=>nativeCell(v.rule,key))];}};}
  return tables;
}
