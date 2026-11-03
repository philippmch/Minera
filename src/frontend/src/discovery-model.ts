import { ilpMinerQuery, geneticMinerQuery } from "./api";
import { backendFetch } from "./backend-fetch";
import { isILPMinerResult, isGeneticMinerResult, type ILPMinerResult, type GeneticMinerResult, type ILPMinerOptions, type GeneticMinerOptions } from "./types";
import { computePetriLayout } from "./petri-layout";

export type DiscoveryFamily = "ilp" | "genetic";
export interface DiscoveryModel { family:"DISCOVERY_MODEL";source_id:string;result:ILPMinerResult|GeneticMinerResult;pnml:string|null;report_json:string;activity_key?:string; }
export interface IlpForm { activity:string;start:string;end:string;alpha:string;noise:string;nodes:string;causal:string; }
export interface GeneticForm { activity:string;timestamp:string;caseId:string;seed:string;population:string;generations:string;elitism:string;crossover:string;mutation:string;tournament:string;activities:string;states:string; }
export const DEFAULT_ILP_FORM:IlpForm={activity:"",start:"▶",end:"■",alpha:"1",noise:"0",nodes:"100000",causal:""};
export const DEFAULT_GENETIC_FORM:GeneticForm={activity:"",timestamp:"",caseId:"",seed:"0",population:"32",generations:"20",elitism:"0.1",crossover:"0.8",mutation:"0.1",tournament:"3",activities:"",states:"10000"};
function integer(text:string,label:string,min:number,max:number):number {const value=Number(text);if(!text.trim()||!Number.isSafeInteger(value)||value<min||value>max)throw new Error(`${label} must be an integer from ${min} through ${max}.`);return value;}
function rate(text:string,label:string):number {const value=Number(text);if(!text.trim()||!Number.isFinite(value)||value<0||value>1)throw new Error(`${label} must be a finite number from 0 through 1.`);return value;}
export function ilpOptions(form:IlpForm):ILPMinerOptions {
  if(!form.start||!form.end)throw new Error("Artificial boundary labels must not be empty.");
  return {activity_key:form.activity||undefined,artificial_start_activity:form.start,artificial_end_activity:form.end,alpha:rate(form.alpha,"Alpha"),noise_threshold:rate(form.noise,"Noise threshold"),max_nodes:integer(form.nodes,"Maximum ILP nodes",1,1000000),causal_relation:form.causal||undefined};
}
/** The genetic miner's hard activity cap (genetic_miner.cpp kHardActivities); a blank field keeps the server default (64). */
export const GENETIC_MAX_ACTIVITIES=256;
export function geneticOptions(form:GeneticForm):GeneticMinerOptions {
  const population=integer(form.population,"Population size",2,10000);const tournament=integer(form.tournament,"Tournament sample",1,10000);
  if(tournament>=population)throw new Error("Tournament sample must be smaller than the population size.");
  return {activity_key:form.activity||undefined,timestamp_key:form.timestamp||undefined,case_id_key:form.caseId||undefined,seed:integer(form.seed,"Seed",0,Number.MAX_SAFE_INTEGER),population_size:population,generations:integer(form.generations,"Generations",0,10000),elitism_rate:rate(form.elitism,"Elitism rate"),crossover_rate:rate(form.crossover,"Crossover rate"),mutation_rate:rate(form.mutation,"Mutation rate"),elitism_min_sample:tournament,max_activities:form.activities.trim()?integer(form.activities,"Maximum activities",1,GENETIC_MAX_ACTIVITIES):undefined,max_states:integer(form.states,"Maximum candidate states",1,1000000)};
}
export function isDiscoveryModel(value:unknown):value is Omit<DiscoveryModel,"report_json"> {
  if(!value||typeof value!=="object")return false;const r=value as Partial<DiscoveryModel>;
  return r.family==="DISCOVERY_MODEL"&&typeof r.source_id==="string"&&!!r.source_id&&(isILPMinerResult(r.result)||isGeneticMinerResult(r.result))&&(r.result.diagnostics.computed?typeof r.pnml==="string"&&r.pnml.includes("<pnml"):r.pnml===null);
}
export async function requestDiscoveryModel(family:DiscoveryFamily,sourceId:string,options:ILPMinerOptions|GeneticMinerOptions,signal?:AbortSignal):Promise<DiscoveryModel> {
  if(!sourceId)throw new Error("Refresh the discovery source first.");
  const query=new URLSearchParams(family==="ilp"?ilpMinerQuery(options as ILPMinerOptions):geneticMinerQuery(options as GeneticMinerOptions));query.set("source_id",sourceId);query.set("include_pnml","true");
  const response=await backendFetch(`/api/${family}-mine?${query}`,{method:"POST",...(signal?{signal}:{})});const raw=await response.text();let value:unknown;
  try{value=JSON.parse(raw);}catch{throw new Error("The backend returned an invalid discovery response.");}
  if(value&&typeof value==="object"&&"error"in value)throw new Error(String((value as {error:unknown}).error));
  if(!response.ok)throw new Error(`Discovery returned ${response.status}.`);
  if(!isDiscoveryModel(value))throw new Error("The backend returned an invalid discovery model.");
  if(value.source_id!==sourceId)throw new Error("The source log changed; refresh the discovery source.");
  if(value.result.family!==(family==="ilp"?"ILP":"GENETIC"))throw new Error("The backend returned a different discovery method.");
  // Insert original native JSON verbatim, retaining exact large integers.
  const report=`{"family":"DISCOVERY_REPORT","request":${JSON.stringify({family,source_id:sourceId,options})},"response":${raw}}`;
  return {...value,report_json:report,activity_key:options.activity_key};
}
export function discoveryLayout(model:DiscoveryModel) {return computePetriLayout(model.result.net,{activities:model.result.net.activities.map(name=>({name}))});}
export function discoveryPnml(model:DiscoveryModel):File {
  if(!model.pnml)throw new Error("This discovery result has no model to export.");
  return new File([model.pnml],`minera-${model.result.family.toLowerCase()}.pnml`,{type:"application/xml"});
}
export function matrixGroups(groups:number[][],activities:string[]):string {return groups.length?groups.map(group=>`(${group.map(id=>activities[id]).join(" OR ")})`).join(" AND "):"None";}
